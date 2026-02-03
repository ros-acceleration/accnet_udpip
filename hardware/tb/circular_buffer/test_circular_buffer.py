"""

Copyright (c) 2023 Juan Manuel Reina Muñoz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

"""

###################################################################################
# Usage
###################################################################################

"""
Alternative 1 (make)
    - Configure "Makefile" file to configure the general test configuration
    - Run "make" from terminal

Alternative 2 (cocotb-test)
    - Configure "test_fpga_core" function located at this file to configure the general test configuration
    - Run "SIM=icarus pytest -o log_cli=True" from terminal
"""

###################################################################################
# Imports
###################################################################################

import logging
import os

import cocotb_test.simulator

import cocotb
from cocotb.log import SimLog
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge


###################################################################################
# TB class (common for all tests)
###################################################################################

class TB:

    BUFFER_LENGTH = 3
    INDEX_WIDTH   = 2

    def __init__(self, dut):

        self.dut = dut
        self.dut.data_pushed_i.value = 0
        self.dut.data_popped_i.value = 0

        self.log = SimLog("cocotb.tb")
        self.log.setLevel(logging.DEBUG)

        self.buffer_data = [None for i in range(TB.BUFFER_LENGTH)] # Buffer data is external to DUT

        cocotb.start_soon(Clock(dut.clk_i, 6.4, units="ns").start())

    async def init(self):

        # Wait for 2 cycles and then force a 2-cycle reset
        self.dut.rst_i.setimmediatevalue(0)
        for k in range(2): await RisingEdge(self.dut.clk_i)
        self.dut.rst_i.value = 1
        for k in range(2): await RisingEdge(self.dut.clk_i)
        self.dut.rst_i.value = 0

    def buffer_print(self):
        self.log.info("Buffer status: " + str(self.buffer_data))

    def assert_buffer(self, buffer_content_expected, head_expected=0, tail_expected=0, full_expected=0, empty_expected=0):
        for pos in range(TB.BUFFER_LENGTH):
            assert(self.buffer_data[pos] == buffer_content_expected[pos])
        assert(self.dut.head_index_o == head_expected)
        assert(self.dut.tail_index_o == tail_expected)
        assert(self.dut.full_o       == full_expected)
        assert(self.dut.empty_o      == empty_expected)

    async def push_to_buffer(self, data):
        self.log.info("Pushing data: " + data + " ...")
        if (not self.dut.full_o.value):
            self.buffer_data[self.dut.head_index_o.value] = data
            self.dut.data_pushed_i.value = 1
            await RisingEdge(self.dut.clk_i)
            self.dut.data_pushed_i.value = 0
            await RisingEdge(self.dut.clk_i)
            self.log.info(data + " successfully pushed")
        else:
            self.log.info("Buffer is full")

    async def pop_from_buffer(self):
        self.log.info("Popping data ...")
        elem_read = None
        if (not self.dut.empty_o.value):
            elem_read = self.buffer_data[self.dut.tail_index_o.value]
            self.buffer_data[self.dut.tail_index_o.value] = None
            self.dut.data_popped_i.value = 1
            await RisingEdge(self.dut.clk_i)
            self.dut.data_popped_i.value = 0
            await RisingEdge(self.dut.clk_i)
            self.log.info("successfully popped: " + elem_read)
        else:
            self.log.info("Buffer is empty")

###################################################################################
# Test: run_test_circular_buffer 
# Stimulus: 
# Expected: 
###################################################################################

@cocotb.test()
async def run_test_circular_buffer(dut):

    # Initialize TB

    tb = TB(dut)
    await tb.init()

    # Initial state of buffer

    tb.log.info("-----------------------------------------------------------------------")
    tb.log.info("Initial state...")
    tb.log.info("Buffer length: " + str(tb.dut.BUFFER_LENGTH.value))
    tb.buffer_print()
    tb.assert_buffer(buffer_content_expected=[None, None, None], empty_expected=1)
    tb.log.info("-----------------------------------------------------------------------")

    # 0 elements in buffer. Push data

    await tb.push_to_buffer("data0")
    tb.buffer_print()
    tb.assert_buffer(buffer_content_expected=["data0", None, None], head_expected=1)
    tb.log.info("-----------------------------------------------------------------------")

    # 1 elements in buffer. Pop data

    await tb.pop_from_buffer()
    tb.buffer_print()
    tb.assert_buffer(buffer_content_expected=[None, None, None], head_expected=1, tail_expected=1, empty_expected=1)
    tb.log.info("-----------------------------------------------------------------------")

    # 0 elements in buffer. Push 4 elements (only 3 should be actually pushed)

    await tb.push_to_buffer("data0")
    await tb.push_to_buffer("data1")
    await tb.push_to_buffer("data2")
    await tb.push_to_buffer("data3")
    tb.buffer_print()
    tb.assert_buffer(buffer_content_expected=["data2", "data0", "data1"], head_expected=1, tail_expected=1, full_expected=1)
    tb.log.info("-----------------------------------------------------------------------")    

    # 3 elements in buffer. Pop 4 elements (only 3 should be actually popped)

    await tb.pop_from_buffer()
    await tb.pop_from_buffer()
    await tb.pop_from_buffer()
    await tb.pop_from_buffer()
    tb.buffer_print()
    tb.assert_buffer(buffer_content_expected=[None, None, None], head_expected=1, tail_expected=1, empty_expected=1)
    tb.log.info("-----------------------------------------------------------------------")    

    # Wait for some cycles at the end to improve waveform readability

    for _ in range(4): await RisingEdge(tb.dut.clk_i)

###################################################################################
# cocotb-test: paths, cocotb and simulator definitions
###################################################################################

tests_dir = os.path.abspath(os.path.dirname(__file__))
rtl_dir = os.path.abspath(os.path.join(tests_dir, '..', '..', 'rtl'))
lib_dir = os.path.abspath(os.path.join(rtl_dir, '..', 'lib'))

def test_fpga_core(request):
    dut = "circular_buffer"
    module = os.path.splitext(os.path.basename(__file__))[0]
    toplevel = dut

    verilog_sources = [
        os.path.join(rtl_dir, f"{dut}.v"),
    ]

    parameters = {}
    parameters['BUFFER_LENGTH'] = 3
    parameters['INDEX_WIDTH'] = 2 

    extra_env = {f'PARAM_{k}': str(v) for k, v in parameters.items()}

    sim_build = os.path.join(tests_dir, "sim_build",
        request.node.name.replace('[', '-').replace(']', ''))

    cocotb_test.simulator.run(
        python_search=[tests_dir],
        verilog_sources=verilog_sources,
        toplevel=toplevel,
        module=module,
        parameters=parameters,
        sim_build=sim_build,
        extra_env=extra_env,
    )
