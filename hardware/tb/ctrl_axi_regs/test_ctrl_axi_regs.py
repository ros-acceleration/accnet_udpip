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
    - Configure "test_ctrl_axi_regs" function located at this file to configure the general test configuration
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

from cocotbext.axi import AxiLiteBus, AxiLiteMaster

import json
import binascii

###################################################################################
# TB class (common for all tests)
###################################################################################

class TB:

    # A = 3

    REGS_ADDR_DIC = {
        # "reg1" : 0x0000, "reg2" : 0x0004,

        # "REG0" : 0x0000,
        # "REG1" : 0x0004,
        # "REG2" : 0x0008,
        # "REG3" : 0x000C,

        "ADDR_MAC_0_N_O" :0x00000010,
        "ADDR_MAC_1_N_O" :0x00000018,
        # "ADDR_IP_0_N_O"  :0x00000020,
    }

    def __init__(self, dut):

        self.dut = dut       

        self.log = SimLog("cocotb.tb")
        self.log.setLevel(logging.DEBUG)

        cocotb.start_soon(Clock(dut.clk_i, 6.4, units="ns").start())

        self.axil_regs = AxiLiteMaster(AxiLiteBus.from_prefix(self.dut, "s_axil"), self.dut.clk_i, self.dut.rst_i, reset_active_level=True)

    async def init(self):

        # Wait for 2 cycles and then force a 2-cycle reset
        self.dut.rst_i.value = 0
        for k in range(2): await RisingEdge(self.dut.clk_i)
        self.dut.rst_i.value = 1
        for k in range(2): await RisingEdge(self.dut.clk_i)
        self.dut.rst_i.value = 0

    async def read_reg32(self, addr):
        data_read = await self.axil_regs.read_dword(addr, byteorder='little')
        data_strhex = format(data_read, '08X')
        return data_strhex
    
    async def read_all(self):
        data_read_dic = {}
        for reg_key in TB.REGS_ADDR_DIC:
            data_read_strhex = await self.read_reg32(TB.REGS_ADDR_DIC[reg_key])
            data_read_dic[reg_key] = data_read_strhex
        return data_read_dic

    async def write_reg32(self, addr, data_strhex):
        write_op = self.axil_regs.init_write(addr, bytes.fromhex(data_strhex[2:]))
        await write_op.wait()
        resp = write_op.data
        return resp

    async def write_all(self, data_strhex):
        data_read_dic = {}
        for reg_key in TB.REGS_ADDR_DIC:
            data_read_dic[reg_key] = await self.write_reg32(TB.REGS_ADDR_DIC[reg_key], data_strhex)

    async def dump_all(self):
        data_read_dic = {}
        data_read_dic = await self.read_all()
        self.log.info("Dumping all registers...")
        self.log.info(json.dumps(data_read_dic, indent=4))

###################################################################################
# Test: run_test_ctrl_axi_regs 
# Stimulus: 
# Expected: 
###################################################################################

@cocotb.test()
async def run_test_ctrl_axi_regs(dut):

    # Initialize TB

    tb = TB(dut)
    await tb.init()

    # Write all 0s

    tb.log.info("-----------------------------------------------------------------------")
    tb.log.info("Write all 0s...")
    await tb.write_all('0x00000000')
    await tb.dump_all()
    tb.log.info("-----------------------------------------------------------------------")

    # Write 1s to all regs and read state

    tb.log.info("Write all 1s...")
    await tb.write_all('0xA0A0A0A0')
    await tb.dump_all()
    tb.log.info("-----------------------------------------------------------------------")

    # Wait for some cycles at the end to improve waveform readability

    for _ in range(4): await RisingEdge(tb.dut.clk_i)

###################################################################################
# cocotb-test: paths, cocotb and simulator definitions
###################################################################################

tests_dir = os.path.abspath(os.path.dirname(__file__))
rtl_dir = os.path.abspath(os.path.join(tests_dir, '..', '..', 'rtl'))
lib_dir = os.path.abspath(os.path.join(rtl_dir, '..', 'lib'))

def test_ctrl_axi_regs(request):
    dut = "ctrl_axi_regs"
    module = os.path.splitext(os.path.basename(__file__))[0]
    toplevel = dut

    verilog_sources = [
        os.path.join(rtl_dir, f"{dut}.v"),
        os.path.join(rtl_dir, "config_regs_AXI_Manager.v"),
        os.path.join(rtl_dir, "pulse_on_posedge.v"),
    ]

    parameters = {}
    # parameters['A'] = 3

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
