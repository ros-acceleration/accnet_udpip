/*

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

*/

// Language: Verilog 2001

`resetall
`timescale 1ns / 1ps
`default_nettype none

/**********************************************************************************
* Module declaration
**********************************************************************************/

module ctrl_axi_regs #(
    parameter C_S_AXI_ADDR_WIDTH   = 14, // 2^14 = 16KB; needed to handle 1024 rx buffers
    parameter C_S_AXI_DATA_WIDTH   = 32,
    parameter C_BUFFRX_INDEX_WIDTH = 5,
    parameter C_BUFFTX_INDEX_WIDTH = 5,
    parameter C_MAX_UDP_PORTS      = 1024
) (
    input    wire                               clk_i           ,
    input    wire                               rst_i           ,
    // AXI Lite signals
    input    wire  [C_S_AXI_ADDR_WIDTH - 1:0]   s_axil_awaddr   ,
    input    wire                               s_axil_awvalid  ,
    output   wire                               s_axil_awready  ,
    input    wire  [C_S_AXI_DATA_WIDTH - 1:0]   s_axil_wdata    ,
    input    wire  [(C_S_AXI_DATA_WIDTH/8)-1:0] s_axil_wstrb    ,
    input    wire                               s_axil_wvalid   ,
    output   wire                               s_axil_wready   ,
    output   wire  [1:0]                        s_axil_bresp    ,
    output   wire                               s_axil_bvalid   ,
    input    wire                               s_axil_bready   ,
    input    wire  [C_S_AXI_ADDR_WIDTH - 1:0]   s_axil_araddr   ,
    input    wire                               s_axil_arvalid  ,
    output   wire                               s_axil_arready  ,
    output   wire  [C_S_AXI_DATA_WIDTH - 1:0]   s_axil_rdata    ,
    output   wire  [1:0]                        s_axil_rresp    ,
    output   wire                               s_axil_rvalid   ,
    input    wire                               s_axil_rready   ,
    // Kernel control signals
    output   wire                               ap_start        ,
    input    wire                               ap_done         ,
    input    wire                               ap_ready        ,
    input    wire                               ap_idle         ,
    output   wire                               interrupt       ,
    // User defined signals
    output   wire                               user_rst_o        ,
    output   wire  [47 : 0]                     local_mac_o       ,
    output   wire  [C_S_AXI_DATA_WIDTH-1 : 0]   gateway_ip_o      ,
    output   wire  [C_S_AXI_DATA_WIDTH-1 : 0]   subnet_mask_o     ,
    output   wire  [C_S_AXI_DATA_WIDTH-1 : 0]   local_ip_o        ,
    output   wire  [C_S_AXI_DATA_WIDTH-1 : 0]   udp_port_range_l_o,
    output   wire  [C_S_AXI_DATA_WIDTH-1 : 0]   udp_port_range_h_o,
    output   wire  [C_S_AXI_DATA_WIDTH-1 : 0]   shared_mem_o      ,

    input    wire  [C_MAX_UDP_PORTS*C_BUFFRX_INDEX_WIDTH-1 : 0 ] bufrx_head_i     ,
    input    wire  [C_MAX_UDP_PORTS*C_BUFFRX_INDEX_WIDTH-1 : 0 ] bufrx_tail_i     ,
    input    wire  [C_MAX_UDP_PORTS                     -1 : 0 ] bufrx_empty_i    ,
    input    wire  [C_MAX_UDP_PORTS                     -1 : 0 ] bufrx_full_i     ,
    input    wire  [C_MAX_UDP_PORTS                     -1 : 0 ] bufrx_pushed_i   ,
    output   wire  [C_MAX_UDP_PORTS                     -1 : 0 ] bufrx_popped_o   ,
    output   wire  [C_MAX_UDP_PORTS                     -1 : 0 ] bufrx_opensock_o ,

    input    wire                               bufrx_push_irq_i  ,
    input    wire  [C_BUFFTX_INDEX_WIDTH : 0]   buftx_head_i      ,
    input    wire  [C_BUFFTX_INDEX_WIDTH : 0]   buftx_tail_i      ,
    input    wire                               buftx_empty_i     ,
    input    wire                               buftx_full_i      ,
    output   wire                               buftx_pushed_o    ,
    input    wire                               buftx_popped_i    
);

/**********************************************************************************
* buffer rx vector handling
**********************************************************************************/

localparam BUFFER_POPPED_OFFSET = 0;
localparam BUFFER_PUSHED_OFFSET = 1;
localparam BUFFER_FULL_OFFSET   = 2;
localparam BUFFER_EMPTY_OFFSET  = 3;
localparam BUFFER_TAIL_OFFSET   = 4;
localparam BUFFER_TAIL_UPPER      = BUFFER_TAIL_OFFSET + C_BUFFRX_INDEX_WIDTH - 1;
localparam BUFFER_HEAD_OFFSET     = BUFFER_TAIL_UPPER + 1;
localparam BUFFER_HEAD_UPPER      = BUFFER_HEAD_OFFSET + C_BUFFRX_INDEX_WIDTH - 1;
localparam BUFFER_OPENSOCK_OFFSET = BUFFER_HEAD_UPPER + 1;
localparam BUFFER_DUMMY_OFFSET    = BUFFER_OPENSOCK_OFFSET + 1;
localparam BUFFER_DUMMY_UPPER     = C_S_AXI_DATA_WIDTH - 1;

wire [C_S_AXI_DATA_WIDTH*C_MAX_UDP_PORTS-1 : 0] buffer_rx_vector;
genvar buffer_index;
generate
    for (buffer_index = 0; buffer_index < C_MAX_UDP_PORTS; buffer_index = buffer_index + 1) begin
        // Inputs
        assign buffer_rx_vector[C_S_AXI_DATA_WIDTH*buffer_index + BUFFER_PUSHED_OFFSET                                                      ] = bufrx_pushed_i[buffer_index];
        assign buffer_rx_vector[C_S_AXI_DATA_WIDTH*buffer_index + BUFFER_FULL_OFFSET                                                        ] = bufrx_full_i  [buffer_index];
        assign buffer_rx_vector[C_S_AXI_DATA_WIDTH*buffer_index + BUFFER_EMPTY_OFFSET                                                       ] = bufrx_empty_i [buffer_index];
        assign buffer_rx_vector[C_S_AXI_DATA_WIDTH*buffer_index + BUFFER_TAIL_UPPER  : C_S_AXI_DATA_WIDTH*buffer_index + BUFFER_TAIL_OFFSET ] = bufrx_tail_i  [C_BUFFRX_INDEX_WIDTH*(buffer_index+1) - 1 : C_BUFFRX_INDEX_WIDTH*buffer_index];
        assign buffer_rx_vector[C_S_AXI_DATA_WIDTH*buffer_index + BUFFER_HEAD_UPPER  : C_S_AXI_DATA_WIDTH*buffer_index + BUFFER_HEAD_OFFSET ] = bufrx_head_i  [C_BUFFRX_INDEX_WIDTH*(buffer_index+1) - 1 : C_BUFFRX_INDEX_WIDTH*buffer_index];
        assign buffer_rx_vector[C_S_AXI_DATA_WIDTH*buffer_index + BUFFER_DUMMY_UPPER : C_S_AXI_DATA_WIDTH*buffer_index + BUFFER_DUMMY_OFFSET] = 0;
        // Outputs
        pulse_on_posedge pulse_bufrx_popped (
            .clk_i (clk_i ),
            .rst_i (rst_i ),
            .signal_rising_i (buffer_rx_vector[C_S_AXI_DATA_WIDTH*buffer_index + BUFFER_POPPED_OFFSET ]),
            .signal_pulse_o  (bufrx_popped_o[buffer_index])
        );
        assign bufrx_opensock_o[buffer_index] = buffer_rx_vector[C_S_AXI_DATA_WIDTH*buffer_index + BUFFER_OPENSOCK_OFFSET ];
    end
endgenerate

/**********************************************************************************
* config_regs_AXI_Manager instance
**********************************************************************************/

wire buftx_pushed_temp;

config_regs_AXI_Manager #(
    .C_S_AXI_ADDR_WIDTH     (C_S_AXI_ADDR_WIDTH    ),
    .C_S_AXI_DATA_WIDTH     (C_S_AXI_DATA_WIDTH    ),
    .C_BUFFRX_INDEX_WIDTH   (C_BUFFRX_INDEX_WIDTH  ),
    .C_BUFFTX_INDEX_WIDTH   (C_BUFFTX_INDEX_WIDTH  ),        
    .C_MAX_UDP_PORTS        (C_MAX_UDP_PORTS       ),
    .BUFFER_POPPED_OFFSET   (BUFFER_POPPED_OFFSET  ),
    .BUFFER_PUSHED_OFFSET   (BUFFER_PUSHED_OFFSET  ),
    .BUFFER_FULL_OFFSET     (BUFFER_FULL_OFFSET    ),
    .BUFFER_EMPTY_OFFSET    (BUFFER_EMPTY_OFFSET   ),
    .BUFFER_TAIL_OFFSET     (BUFFER_TAIL_OFFSET    ),
    .BUFFER_TAIL_UPPER      (BUFFER_TAIL_UPPER     ),
    .BUFFER_HEAD_OFFSET     (BUFFER_HEAD_OFFSET    ),
    .BUFFER_HEAD_UPPER      (BUFFER_HEAD_UPPER     ),
    .BUFFER_OPENSOCK_OFFSET (BUFFER_OPENSOCK_OFFSET),
    .BUFFER_DUMMY_OFFSET    (BUFFER_DUMMY_OFFSET   ),
    .BUFFER_DUMMY_UPPER     (BUFFER_DUMMY_UPPER    )
) config_regs_AXI_Manager_inst (
    .clk                (clk_i  ),
    .res_n              (~rst_i ),
    .AWADDR             (s_axil_awaddr  ),
    .AWVALID            (s_axil_awvalid ),
    .AWREADY            (s_axil_awready ),
    .WDATA              (s_axil_wdata   ),
    .WSTRB              (s_axil_wstrb   ),
    .WVALID             (s_axil_wvalid  ),
    .WREADY             (s_axil_wready  ),
    .BRESP              (s_axil_bresp   ),
    .BVALID             (s_axil_bvalid  ),
    .BREADY             (s_axil_bready  ),
    .ARADDR             (s_axil_araddr  ),
    .ARVALID            (s_axil_arvalid ),
    .ARREADY            (s_axil_arready ),
    .RDATA              (s_axil_rdata   ),
    .RRESP              (s_axil_rresp   ),
    .RVALID             (s_axil_rvalid  ),
    .RREADY             (s_axil_rready  ),
    .ap_start           (ap_start       ),
    .ap_done            (ap_done        ),
    .ap_ready           (ap_ready       ),
    .ap_idle            (ap_idle        ),
    .interrupt          (interrupt      ),
    .user_rst_o         (user_rst_o         ),
    .local_mac_o        (local_mac_o        ),
    .gateway_ip_o       (gateway_ip_o       ),
    .subnet_mask_o      (subnet_mask_o      ),
    .local_ip_o         (local_ip_o         ),
    .udp_port_range_l_o (udp_port_range_l_o ),
    .udp_port_range_h_o (udp_port_range_h_o ),
    .shared_mem_o       (shared_mem_o       ),
    .buffer_rx_vector_io(buffer_rx_vector   ),
    .bufrx_push_irq_i   (bufrx_push_irq_i   ),
    .buftx_head_i       (buftx_head_i       ),
    .buftx_tail_i       (buftx_tail_i       ),
    .buftx_empty_i      (buftx_empty_i      ),
    .buftx_full_i       (buftx_full_i       ),
    .buftx_pushed_o     (buftx_pushed_temp  ),
    .buftx_popped_i     (buftx_popped_i     )
);

/**********************************************************************************
* tx buffer pushed: pulse generation on rising edge
**********************************************************************************/

pulse_on_posedge pulse_buftx_pushed (
    .clk_i (clk_i ),
    .rst_i (rst_i ),
    .signal_rising_i (buftx_pushed_temp),
    .signal_pulse_o  (buftx_pushed_o)
);

endmodule

