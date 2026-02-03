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

`resetall
`timescale 1ns / 1ps
`default_nettype none

/**********************************************************************************
 * Includes
 **********************************************************************************/

`include "utils.v"

/**********************************************************************************
 * controller module
 *   - Provides an slave axi lite interface (s_axil_ctrl) for synchronization with PS
 *   - s_axil_ctrl is connected to a set of registers
 *   - Handles udp_ip parameters (IP, MAC, etc.)
 *   - Handles the control signals for axi_dma_rd/wr (address, size, start) 
 *   - Handles an externally controlled reset and feeds it to the modules requiring it
 *
 * Order of buffers in DDR:
 *   - First rx buffer is placed in DDR at shared_mem_base_address
 *   - Next rx buffers are placed contiguously, being buffer_rx[i] located at shared_mem_base_address + (MAX_UDP_PORTS-1)*BUFFER_RX_LENGTH*BUFFER_ELEM_MAX_SIZE
 *   - Tx buffer is placed in DDR at shared_mem_base_address + MAX_UDP_PORTS*BUFFER_RX_LENGTH*BUFFER_ELEM_MAX_SIZE
 **********************************************************************************/

module controller #(
    parameter DMA_ADDR_WIDTH       = 32,
    parameter DMA_LEN_WIDTH        = 20,
    parameter BUFFER_RX_LENGTH     = 32,
    parameter BUFFER_TX_LENGTH     = 32,
    parameter BUFFER_ELEM_MAX_SIZE = 2*1024, // 2KB per slot in buffer
    parameter HEADER_NUM_WORDS     = 5,
    parameter MAX_UDP_PORTS        = 1024
) (

    // General
    input  wire         clk_i          ,
    input  wire         rst_i          ,
    output wire         rst_o          ,

    // Slave AXI lite (for synchonization with PS)
    input  wire [31:00] s_axil_awaddr ,
    input  wire         s_axil_awvalid,
    output wire         s_axil_awready,
    input  wire [31:00] s_axil_wdata  ,
    input  wire [03:00] s_axil_wstrb  ,
    input  wire         s_axil_wvalid ,
    output wire         s_axil_wready ,
    output wire [01:00] s_axil_bresp  ,
    output wire         s_axil_bvalid ,
    input  wire         s_axil_bready ,
    input  wire [31:00] s_axil_araddr ,
    input  wire         s_axil_arvalid,
    output wire         s_axil_arready,
    output wire [31:00] s_axil_rdata  ,
    output wire [01:00] s_axil_rresp  ,
    output wire         s_axil_rvalid ,
    input  wire         s_axil_rready ,

    // udp_complete
    output reg  [47:00] local_mac             ,
    output reg  [31:00] gateway_ip            ,
    output reg  [31:00] subnet_mask           ,
    output reg  [31:00] local_ip              ,
    output wire         rx_hdr_ready          ,
    input  wire         rx_hdr_valid          ,
    input  wire [31:00] rx_hdr_source_ip      ,
    input  wire [15:00] rx_hdr_source_port    ,
    input  wire [31:00] rx_hdr_dest_ip        ,
    input  wire [15:00] rx_hdr_dest_port      ,
    input  wire [15:00] rx_hdr_udp_length     ,
    output wire         rx_reduced_payload_axis_tready,
    input  wire         rx_reduced_payload_axis_tvalid,
    input  wire [07:00] rx_reduced_payload_axis_tdata ,
    input  wire         rx_reduced_payload_axis_tlast ,
    input  wire         rx_reduced_payload_axis_tuser ,
    input  wire         tx_hdr_ready          ,
    output wire         tx_hdr_valid          ,
    output wire [31:00] tx_hdr_source_ip      ,
    output wire [15:00] tx_hdr_source_port    ,
    output wire [31:00] tx_hdr_dest_ip        ,
    output wire [15:00] tx_hdr_dest_port      ,    
    output wire [15:00] tx_hdr_udp_length     ,
    input  wire         tx_reduced_payload_axis_tready,
    output wire         tx_reduced_payload_axis_tvalid,
    output wire [07:00] tx_reduced_payload_axis_tdata ,
    output wire         tx_reduced_payload_axis_tlast ,
    output wire         tx_reduced_payload_axis_tuser ,
    
    // AXI DMA read
    output wire [DMA_ADDR_WIDTH-1 : 00] dma_rd_ctrl_addr_o      ,
    output wire [DMA_LEN_WIDTH-1  : 00] dma_rd_ctrl_len_bytes_o ,
    output reg                          dma_rd_ctrl_valid_o     ,
    input  wire                         dma_rd_ctrl_ready_i     ,
    input  wire                         dma_rd_ctrl_popped_i    , // Must go high for one single pulse
    output wire                         dma_rd_data_axis_tready ,
    input  wire                         dma_rd_data_axis_tvalid ,
    input  wire                         dma_rd_data_axis_tlast  ,
    input  wire [63               : 00] dma_rd_data_axis_tdata  ,
    input  wire [07               : 00] dma_rd_data_axis_tkeep  ,
    input  wire                         dma_rd_data_axi_valid   ,

    // AXI DMA write
    output wire [DMA_ADDR_WIDTH-1 : 00] dma_wr_ctrl_addr_o      ,
    output reg  [DMA_LEN_WIDTH-1  : 00] dma_wr_ctrl_len_bytes_o ,
    output reg                          dma_wr_ctrl_valid_o     ,
    input  wire                         dma_wr_ctrl_ready_i     ,
    input  wire                         dma_wr_ctrl_pushed_i    , // Must go high for one single pulse
    input  wire                         dma_wr_data_axis_tready ,
    output wire                         dma_wr_data_axis_tvalid ,
    output wire                         dma_wr_data_axis_tlast  ,
    output wire [63               : 00] dma_wr_data_axis_tdata  ,
    output wire [07               : 00] dma_wr_data_axis_tkeep  ,
    input  wire                         dma_wr_data_axi_last    ,

    output wire                         buffer_rx_pushed_interr_o // Level interrupt; high when rx pushed detected; cleared from PS

);

/**********************************************************************************
* General declarations
**********************************************************************************/

wire rst_user;
wire rst_global;

reg [31:00] udp_port_range_l;
reg [31:00] udp_port_range_h;

reg  [DMA_ADDR_WIDTH-1 : 00] shared_mem_base_address;

localparam BUFFRX_INDEX_WIDTH = log2(BUFFER_RX_LENGTH);
localparam BUFFTX_INDEX_WIDTH = log2(BUFFER_TX_LENGTH);

/**********************************************************************************
* Registers for PL-PS communication
**********************************************************************************/

wire rst_user_req;

wire [DMA_ADDR_WIDTH-1 : 00] shmem_from_ps;

wire [47:00] local_mac_from_ps  ;
wire [31:00] gateway_ip_from_ps ;
wire [31:00] subnet_mask_from_ps;
wire [31:00] local_ip_from_ps   ;
wire [31:00] udp_port_range_l_from_ps;
wire [31:00] udp_port_range_h_from_ps;

always @ (posedge clk_i) begin
    if (rst_global) begin
        local_mac               <= local_mac_from_ps  ;
        gateway_ip              <= gateway_ip_from_ps ;
        subnet_mask             <= subnet_mask_from_ps;
        local_ip                <= local_ip_from_ps   ;
        udp_port_range_l        <= udp_port_range_l_from_ps;
        udp_port_range_h        <= udp_port_range_h_from_ps;
        shared_mem_base_address <= shmem_from_ps;
    end
end

ctrl_axi_regs #(
    .C_S_AXI_DATA_WIDTH   (32 ),
    .C_BUFFRX_INDEX_WIDTH (BUFFRX_INDEX_WIDTH),
    .C_BUFFTX_INDEX_WIDTH (BUFFTX_INDEX_WIDTH),
    .C_MAX_UDP_PORTS      (MAX_UDP_PORTS)
) ctrl_axi_regs_inst (
    .clk_i          (clk_i ),
    .rst_i          (rst_i ),
    // AXI Lite signals
    .s_axil_awaddr  (s_axil_awaddr  ),
    .s_axil_awvalid (s_axil_awvalid ),
    .s_axil_awready (s_axil_awready ),
    .s_axil_wdata   (s_axil_wdata   ),
    .s_axil_wstrb   (s_axil_wstrb   ),
    .s_axil_wvalid  (s_axil_wvalid  ),
    .s_axil_wready  (s_axil_wready  ),
    .s_axil_bresp   (s_axil_bresp   ),
    .s_axil_bvalid  (s_axil_bvalid  ),
    .s_axil_bready  (s_axil_bready  ),
    .s_axil_araddr  (s_axil_araddr  ),
    .s_axil_arvalid (s_axil_arvalid ),
    .s_axil_arready (s_axil_arready ),
    .s_axil_rdata   (s_axil_rdata   ),
    .s_axil_rresp   (s_axil_rresp   ),
    .s_axil_rvalid  (s_axil_rvalid  ),
    .s_axil_rready  (s_axil_rready  ),
    // Kernel control signals
    .ap_start          (),
    .ap_done           (1'b0),
    .ap_ready          (1'b1),
    .ap_idle           (1'b0),
    .interrupt         (buffer_rx_pushed_interr_o),
    // User defined signals
    .user_rst_o        (rst_user_req),
    .local_mac_o       (local_mac_from_ps  ),
    .gateway_ip_o      (gateway_ip_from_ps ),
    .subnet_mask_o     (subnet_mask_from_ps),
    .local_ip_o        (local_ip_from_ps   ),
    .udp_port_range_l_o(udp_port_range_l_from_ps),
    .udp_port_range_h_o(udp_port_range_h_from_ps),
    .shared_mem_o      (shmem_from_ps      ),
    .bufrx_head_i      (circbuff_rx_head_index_vec ),
    .bufrx_tail_i      (circbuff_rx_tail_index_vec ),
    .bufrx_empty_i     (circbuff_rx_empty_vec      ),
    .bufrx_full_i      (circbuff_rx_full_vec       ),
    .bufrx_pushed_i    (circbuff_rx_data_pushed_vec),
    .bufrx_popped_o    (circbuff_rx_data_popped_vec),
    .bufrx_opensock_o  (circbuff_rx_data_opensock_vec),
    .bufrx_push_irq_i  (circbuff_rx_data_pushed_vec_interr),
    .buftx_head_i      (circbuff_tx_head_index ),
    .buftx_tail_i      (circbuff_tx_tail_index ),
    .buftx_empty_i     (circbuff_tx_empty      ),
    .buftx_full_i      (circbuff_tx_full       ),
    .buftx_pushed_o    (circbuff_tx_data_pushed),
    .buftx_popped_i    (circbuff_tx_data_popped) 
);

/**********************************************************************************
* User reset
**********************************************************************************/

assign rst_global = rst_i | rst_user;

reg rx_busy;
always @ (posedge clk_i) begin
    if      (rst_global             ) rx_busy = 0;
    else if (dma_wr_data_axis_tready && dma_wr_data_axis_tvalid ) rx_busy = 1; // Becomes busy when udp_ip start providing new data
    else if (dma_wr_data_axi_last   ) rx_busy = 0; // Out of busy when dma_wr has finished the transfer to DDR
end

// tx goes out of busy when the packet has been totally sent to udp_ip and the dma_rd has finished bringing data. As the dma_rd transfer can be
// longer than the actual stream transferred to the udp_ip (dma_rd always bring the whole buffer slot regardless the actual packet length inside),
// it may happen that the dma_rd is still bringing data but the udpcomplete has received all the valid payload
reg tx_busy, tx_busy_out;
wire tx_busy_out_cond1 = tx_payload_axis_tready && tx_payload_axis_tvalid && tx_payload_axis_tlast;
wire tx_busy_out_cond2 = circbuff_tx_data_popped;
always @ (posedge clk_i) begin
    if      (rst_global                             ) tx_busy_out = 0;
    else if (tx_busy_out_cond1 || tx_busy_out_cond2 ) tx_busy_out = 1;
    else if (!tx_busy                               ) tx_busy_out = 0;
end
always @ (posedge clk_i) begin
    if      (rst_global                                             ) tx_busy = 0;
    else if (dma_rd_data_axi_valid                                  ) tx_busy = 1; // Becomes busy when dma_rd start bringing new data
    else if (tx_busy_out && (tx_busy_out_cond1 || tx_busy_out_cond2)) tx_busy = 0;
end

user_rst_handler user_rst_handler_dut (
    .clk_i        (clk_i       ),
    .rst_user_i   (rst_user_req),
    .rst_por_i    (rst_i       ),
    .rx_busy_i    (rx_busy     ),
    .tx_busy_i    (tx_busy     ),
    .rst_user_o   (rst_user    )
);

/**********************************************************************************
* UDP port filter
**********************************************************************************/

wire [log2(MAX_UDP_PORTS)-1 : 0]    buffer_select_idx;
wire                                valid_udp_port;

wire         portfilt_axis_tready;
wire         portfilt_axis_tvalid;
wire [63:00] portfilt_axis_tdata ;
wire [07:00] portfilt_axis_tkeep ;
wire         portfilt_axis_tlast ;
wire         portfilt_axis_tuser ;

axis_udp_port_filter #(
    .MAX_UDP_PORTS (MAX_UDP_PORTS )
) axis_udp_port_filter_inst (
    .clk                    (clk_i                        ),
    .rst                    (rst_global                   ),
    .hdr_valid              (rx_hdr_valid                 ),
    .hdr_dest_port          (rx_hdr_dest_port             ),
    .dma_done_i             (~rx_busy                     ),
    .udp_port_range_lower   (udp_port_range_l             ),
    .udp_port_range_upper   (udp_port_range_h             ),
    .open_sockets_vector    (circbuff_rx_data_opensock_vec),
    .buffer_select_idx_o    (buffer_select_idx            ),
    .valid_udp_port_o       (valid_udp_port               ),
    .s_axis_payload_tready  (rx_payload_axis_tready       ),
    .s_axis_payload_tvalid  (rx_payload_axis_tvalid       ),
    .s_axis_payload_tdata   (rx_payload_axis_tdata        ),
    .s_axis_payload_tkeep   (rx_payload_axis_tkeep        ),
    .s_axis_payload_tlast   (rx_payload_axis_tlast        ),
    .s_axis_payload_tuser   (rx_payload_axis_tuser        ),
    .m_axis_tready          (portfilt_axis_tready         ),
    .m_axis_tvalid          (portfilt_axis_tvalid         ),
    .m_axis_tdata           (portfilt_axis_tdata          ),
    .m_axis_tkeep           (portfilt_axis_tkeep          ),
    .m_axis_tlast           (portfilt_axis_tlast          ),
    .m_axis_tuser           (portfilt_axis_tuser          )
);

/**********************************************************************************
* Circular buffer rx
**********************************************************************************/

reg                           circbuff_rx_data_pushed_arr  [0 : MAX_UDP_PORTS-1];
wire                          circbuff_rx_data_popped_arr  [0 : MAX_UDP_PORTS-1];
wire                          circbuff_rx_data_opensock_arr[0 : MAX_UDP_PORTS-1];
wire [BUFFRX_INDEX_WIDTH-1:0] circbuff_rx_head_index_arr   [0 : MAX_UDP_PORTS-1];
wire [BUFFRX_INDEX_WIDTH-1:0] circbuff_rx_tail_index_arr   [0 : MAX_UDP_PORTS-1];
wire                          circbuff_rx_full_arr         [0 : MAX_UDP_PORTS-1];
wire                          circbuff_rx_empty_arr        [0 : MAX_UDP_PORTS-1];

genvar buffer_rx_index;
generate
    for (buffer_rx_index = 0; buffer_rx_index < MAX_UDP_PORTS; buffer_rx_index = buffer_rx_index + 1) begin
        circular_buffer #(
            .BUFFER_LENGTH (BUFFER_RX_LENGTH   ),
            .INDEX_WIDTH   (BUFFRX_INDEX_WIDTH )
        ) circular_buffer_rx (
            .clk_i         (clk_i      ),
            .rst_i         (rst_global ),
            .data_pushed_i (circbuff_rx_data_pushed_arr[buffer_rx_index] ),
            .data_popped_i (circbuff_rx_data_popped_arr[buffer_rx_index] ),
            .head_index_o  (circbuff_rx_head_index_arr [buffer_rx_index] ),
            .tail_index_o  (circbuff_rx_tail_index_arr [buffer_rx_index] ),
            .full_o        (circbuff_rx_full_arr       [buffer_rx_index] ),
            .empty_o       (circbuff_rx_empty_arr      [buffer_rx_index] ) 
        );
    end
endgenerate

reg [log2(MAX_UDP_PORTS) : 0] buffer_rx_index2;
always @(*) begin
    for (buffer_rx_index2 = 0; buffer_rx_index2 < MAX_UDP_PORTS; buffer_rx_index2 = buffer_rx_index2 + 1) begin
        if (buffer_rx_index2 == buffer_select_idx) circbuff_rx_data_pushed_arr[buffer_rx_index2] <= dma_wr_ctrl_pushed_i;
        else                                       circbuff_rx_data_pushed_arr[buffer_rx_index2] <= 0;
    end
end

// array to vector

wire [MAX_UDP_PORTS-1                    : 0] circbuff_rx_data_pushed_vec;
wire [MAX_UDP_PORTS-1                    : 0] circbuff_rx_data_popped_vec;
wire [MAX_UDP_PORTS-1                    : 0] circbuff_rx_data_opensock_vec;
wire [MAX_UDP_PORTS*BUFFRX_INDEX_WIDTH-1 : 0] circbuff_rx_head_index_vec ;
wire [MAX_UDP_PORTS*BUFFRX_INDEX_WIDTH-1 : 0] circbuff_rx_tail_index_vec ;
wire [MAX_UDP_PORTS-1                    : 0] circbuff_rx_full_vec       ;
wire [MAX_UDP_PORTS-1                    : 0] circbuff_rx_empty_vec      ;

wire circbuff_rx_data_pushed_vec_interr;
assign circbuff_rx_data_pushed_vec_interr = |circbuff_rx_data_pushed_vec;

genvar buffer_rx_vec_index;
generate
    for (buffer_rx_vec_index = 0; buffer_rx_vec_index < MAX_UDP_PORTS; buffer_rx_vec_index = buffer_rx_vec_index + 1) begin
        assign circbuff_rx_data_pushed_vec  [buffer_rx_vec_index] = circbuff_rx_data_pushed_arr[buffer_rx_vec_index];
        assign circbuff_rx_data_popped_arr  [buffer_rx_vec_index] = circbuff_rx_data_popped_vec[buffer_rx_vec_index];
        assign circbuff_rx_data_opensock_arr[buffer_rx_vec_index] = circbuff_rx_data_opensock_vec[buffer_rx_vec_index];
        assign circbuff_rx_head_index_vec   [(buffer_rx_vec_index+1)*BUFFRX_INDEX_WIDTH-1 : buffer_rx_vec_index*BUFFRX_INDEX_WIDTH] = circbuff_rx_head_index_arr [buffer_rx_vec_index];
        assign circbuff_rx_tail_index_vec   [(buffer_rx_vec_index+1)*BUFFRX_INDEX_WIDTH-1 : buffer_rx_vec_index*BUFFRX_INDEX_WIDTH] = circbuff_rx_tail_index_arr [buffer_rx_vec_index];
        assign circbuff_rx_full_vec         [buffer_rx_vec_index] = circbuff_rx_full_arr       [buffer_rx_vec_index];
        assign circbuff_rx_empty_vec        [buffer_rx_vec_index] = circbuff_rx_empty_arr      [buffer_rx_vec_index];
    end
endgenerate

/**********************************************************************************
* Circular buffer tx
**********************************************************************************/

wire                          circbuff_tx_data_pushed;
wire                          circbuff_tx_data_popped;
wire [BUFFTX_INDEX_WIDTH-1:0] circbuff_tx_head_index ;
wire [BUFFTX_INDEX_WIDTH-1:0] circbuff_tx_tail_index ;
wire                          circbuff_tx_full       ;
wire                          circbuff_tx_empty      ;
assign circbuff_tx_data_popped = dma_rd_ctrl_popped_i;

circular_buffer #(
    .BUFFER_LENGTH (BUFFER_TX_LENGTH   ),
    .INDEX_WIDTH   (BUFFTX_INDEX_WIDTH )
) circular_buffer_tx (
    .clk_i         (clk_i      ),
    .rst_i         (rst_global ),
    .data_pushed_i (circbuff_tx_data_pushed ),
    .data_popped_i (circbuff_tx_data_popped ),
    .head_index_o  (circbuff_tx_head_index  ),
    .tail_index_o  (circbuff_tx_tail_index  ),
    .full_o        (circbuff_tx_full        ),
    .empty_o       (circbuff_tx_empty       ) 
);

/**********************************************************************************
* AXIS header adder
**********************************************************************************/

wire         rx_payload_axis_tready;
wire         rx_payload_axis_tvalid;
wire [63:00] rx_payload_axis_tdata ;
wire [07:00] rx_payload_axis_tkeep ;
wire         rx_payload_axis_tlast ;
wire         rx_payload_axis_tuser ;

axis_adapter #(
    .S_DATA_WIDTH (8),
    .M_DATA_WIDTH (64)
) axis_adapter_adder_inst (
    .clk              (clk_i ),
    .rst              (rst_global ),
    .s_axis_tdata     (rx_reduced_payload_axis_tdata ),
    .s_axis_tvalid    (rx_reduced_payload_axis_tvalid  ),
    .s_axis_tready    (rx_reduced_payload_axis_tready  ),
    .s_axis_tlast     (rx_reduced_payload_axis_tlast  ),
    .s_axis_tuser     (rx_reduced_payload_axis_tuser  ),
    .m_axis_tdata     (rx_payload_axis_tdata  ),
    .m_axis_tkeep     (rx_payload_axis_tkeep  ),
    .m_axis_tvalid    (rx_payload_axis_tvalid ),
    .m_axis_tready    (rx_payload_axis_tready ),
    .m_axis_tlast     (rx_payload_axis_tlast  ),
    .m_axis_tuser     (rx_payload_axis_tuser  )
);

axis_header_adder #(
    .HEADER_NUM_WORDS          (HEADER_NUM_WORDS)
) axis_header_adder_inst (
    .clk                       (clk_i      ),
    .rst                       (rst_global ),
    .hdr_ready                 (rx_hdr_ready ),
    .hdr_valid_port            (valid_udp_port),
    .hdr_valid                 (rx_hdr_valid ),
    .hdr_source_ip             (rx_hdr_source_ip  ),
    .hdr_source_port           (rx_hdr_source_port),
    .hdr_dest_ip               (rx_hdr_dest_ip    ),
    .hdr_dest_port             (rx_hdr_dest_port  ),
    .hdr_udp_length            (rx_hdr_udp_length ),
    .s_axis_payload_tready     (portfilt_axis_tready ),
    .s_axis_payload_tvalid     (portfilt_axis_tvalid ),
    .s_axis_payload_tdata      (portfilt_axis_tdata  ),
    .s_axis_payload_tkeep      (portfilt_axis_tkeep  ),
    .s_axis_payload_tlast      (portfilt_axis_tlast  ),
    .s_axis_payload_tuser      (portfilt_axis_tuser  ),
    .m_axis_tready             (dma_wr_data_axis_tready ),
    .m_axis_tvalid             (dma_wr_data_axis_tvalid ),
    .m_axis_tdata              (dma_wr_data_axis_tdata  ),
    .m_axis_tkeep              (dma_wr_data_axis_tkeep  ),
    .m_axis_tlast              (dma_wr_data_axis_tlast  ),
    .m_axis_tuser              (                        )
);

/**********************************************************************************
* AXIS header remover
**********************************************************************************/

wire         tx_payload_axis_tready;
wire         tx_payload_axis_tvalid;
wire [63:00] tx_payload_axis_tdata ;
wire [07:00] tx_payload_axis_tkeep ;
wire         tx_payload_axis_tlast ;
wire         tx_payload_axis_tuser ; 

axis_adapter #(
    .S_DATA_WIDTH (64),
    .M_DATA_WIDTH (8)
) axis_adapter_remover_inst (
    .clk              (clk_i ),
    .rst              (rst_global ),
    .s_axis_tdata     (tx_payload_axis_tdata  ),
    .s_axis_tkeep     (tx_payload_axis_tkeep  ),
    .s_axis_tvalid    (tx_payload_axis_tvalid  ),
    .s_axis_tready    (tx_payload_axis_tready  ),
    .s_axis_tlast     (tx_payload_axis_tlast   ),
    .s_axis_tuser     (tx_payload_axis_tuser   ),
    .m_axis_tdata     (tx_reduced_payload_axis_tdata  ),
    .m_axis_tvalid    (tx_reduced_payload_axis_tvalid ),
    .m_axis_tready    (tx_reduced_payload_axis_tready ),
    .m_axis_tlast     (tx_reduced_payload_axis_tlast  ),
    .m_axis_tuser     (tx_reduced_payload_axis_tuser  )
);

axis_header_remover #(
    .HEADER_NUM_WORDS  (HEADER_NUM_WORDS    ),
    .TRANS_MAX_LENGTH  (BUFFER_ELEM_MAX_SIZE)
) axis_header_remover_inst (
    .clk               (clk_i     ),
    .rst               (rst_global),
    .s_axis_tready     (dma_rd_data_axis_tready),
    .s_axis_tvalid     (dma_rd_data_axis_tvalid),
    .s_axis_tdata      (dma_rd_data_axis_tdata ),
    .s_axis_tkeep      (dma_rd_data_axis_tkeep ),
    .s_axis_tlast      (dma_rd_data_axis_tlast ),
    .s_axis_tuser      (1'b0                   ),
    .hdr_ready         (tx_hdr_ready      ),
    .hdr_valid         (tx_hdr_valid      ),
    .hdr_source_ip     (tx_hdr_source_ip  ),
    .hdr_source_port   (tx_hdr_source_port),
    .hdr_dest_ip       (tx_hdr_dest_ip    ),
    .hdr_dest_port     (tx_hdr_dest_port  ),
    .hdr_udp_length    (tx_hdr_udp_length ),
    .m_axis_tready     (tx_payload_axis_tready),
    .m_axis_tvalid     (tx_payload_axis_tvalid),
    .m_axis_tdata      (tx_payload_axis_tdata ),
    .m_axis_tkeep      (tx_payload_axis_tkeep ),
    .m_axis_tlast      (tx_payload_axis_tlast ),
    .m_axis_tuser      (tx_payload_axis_tuser )
);

/**********************************************************************************
* DMA write (UDP rx) control
**********************************************************************************/

reg dma_wr_ctrl_valid;
always @ (posedge clk_i) begin
    if      (rst_global || !dma_wr_ctrl_ready_i) dma_wr_ctrl_valid_o = 0;
    else if (dma_wr_data_axis_tvalid           ) dma_wr_ctrl_valid_o = 1;
end

localparam BUFFER_SIZE_BYTES = BUFFER_RX_LENGTH * BUFFER_ELEM_MAX_SIZE;
wire [DMA_ADDR_WIDTH-1 : 00] buffer_rx_0_base_addr;
reg [DMA_ADDR_WIDTH-1 : 00] buffer_rx_selected_base_addr;
reg [DMA_ADDR_WIDTH-1 : 00] buffer_rx_selected_next_slot_addr; 
assign buffer_rx_0_base_addr = shared_mem_base_address; 
always @(posedge clk_i) begin
    buffer_rx_selected_base_addr = buffer_rx_0_base_addr + buffer_select_idx * BUFFER_SIZE_BYTES; 
    buffer_rx_selected_next_slot_addr <= buffer_rx_selected_base_addr + circbuff_rx_head_index_arr[buffer_select_idx] * BUFFER_ELEM_MAX_SIZE;
end 
assign dma_wr_ctrl_addr_o = buffer_rx_selected_next_slot_addr;

wire [DMA_LEN_WIDTH-1: 00] packet_length_bytes;
assign packet_length_bytes = rx_hdr_udp_length + HEADER_NUM_WORDS*8; // the header takes 5 8-byte words
always @ (*) begin
    if (packet_length_bytes <= BUFFER_ELEM_MAX_SIZE) dma_wr_ctrl_len_bytes_o <= packet_length_bytes;
    else                                             dma_wr_ctrl_len_bytes_o <= BUFFER_ELEM_MAX_SIZE;
end

/**********************************************************************************
* DMA read (UDP tx) control
**********************************************************************************/

reg dma_rd_locked;
wire dma_rd_unlock_condition;
assign dma_rd_unlock_condition = dma_rd_data_axis_tlast && dma_rd_data_axis_tvalid && dma_rd_data_axis_tready;
wire dma_rd_lock_condition;
assign dma_rd_lock_condition = dma_rd_ctrl_valid_o && dma_rd_ctrl_ready_i;
always @ (posedge clk_i) begin
    if      (rst_global || dma_rd_unlock_condition) dma_rd_locked = 0;
    else if (dma_rd_lock_condition                ) dma_rd_locked = 1;
end
always @ (posedge clk_i) begin
    if      (rst_global || dma_rd_locked) dma_rd_ctrl_valid_o = 0;
    else if (!circbuff_tx_empty         ) dma_rd_ctrl_valid_o = 1;
end

wire [DMA_ADDR_WIDTH-1 : 00] circbuff_tx_base_addr;
reg [DMA_ADDR_WIDTH-1 : 00] buffer_tx_next_slot_addr;
assign circbuff_tx_base_addr = shared_mem_base_address + BUFFER_SIZE_BYTES * MAX_UDP_PORTS;
always @(posedge clk_i) buffer_tx_next_slot_addr <= circbuff_tx_base_addr + circbuff_tx_tail_index * BUFFER_ELEM_MAX_SIZE; 

assign dma_rd_ctrl_addr_o = buffer_tx_next_slot_addr;
assign dma_rd_ctrl_len_bytes_o = BUFFER_ELEM_MAX_SIZE; // Always reads the whole buffer slot regardless the actual packet size

endmodule