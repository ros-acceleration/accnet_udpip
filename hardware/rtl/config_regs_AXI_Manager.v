/*
 * Copyright 2023 Giulio Corradi giuliocorradi@yahoo.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * file      config_regs_AXI_Manager.v
 * date      Thu May 25 18:24:56 2023 CEST
 */

// Language: Verilog 2001

`resetall
`timescale 1ns / 1ps
`default_nettype none

`include "utils.v"

/**********************************************************************************
* Module declaration
**********************************************************************************/

module config_regs_AXI_Manager #(
    parameter C_S_AXI_ADDR_WIDTH   = 14,
    parameter C_S_AXI_DATA_WIDTH   = 32,
    parameter C_BUFFRX_INDEX_WIDTH = 5,
    parameter C_BUFFTX_INDEX_WIDTH = 5,        
    parameter C_MAX_UDP_PORTS      = 1024,
    parameter BUFFER_POPPED_OFFSET = 0,
    parameter BUFFER_PUSHED_OFFSET = 1,
    parameter BUFFER_FULL_OFFSET   = 2,
    parameter BUFFER_EMPTY_OFFSET  = 3,
    parameter BUFFER_TAIL_OFFSET   = 4,
    parameter BUFFER_TAIL_UPPER      = BUFFER_TAIL_OFFSET + C_BUFFRX_INDEX_WIDTH - 1,
    parameter BUFFER_HEAD_OFFSET     = BUFFER_TAIL_UPPER + 1,
    parameter BUFFER_HEAD_UPPER      = BUFFER_HEAD_OFFSET + C_BUFFRX_INDEX_WIDTH - 1,
    parameter BUFFER_OPENSOCK_OFFSET = BUFFER_HEAD_UPPER + 1,
    parameter BUFFER_DUMMY_OFFSET    = BUFFER_OPENSOCK_OFFSET + 1,
    parameter BUFFER_DUMMY_UPPER     = C_S_AXI_DATA_WIDTH - 1
) (
    input    wire                                               clk               ,
    input    wire                                               res_n             ,
    // AXI Lite signals
    input    wire  [C_S_AXI_ADDR_WIDTH - 1:0]                   AWADDR            ,
    input    wire                                               AWVALID           ,
    output   wire                                               AWREADY           ,
    input    wire  [C_S_AXI_DATA_WIDTH - 1:0]                   WDATA             ,
    input    wire  [(C_S_AXI_DATA_WIDTH/8)-1:0]                 WSTRB             ,
    input    wire                                               WVALID            ,
    output   wire                                               WREADY            ,
    output   wire  [1:0]                                        BRESP             ,
    output   wire                                               BVALID            ,
    input    wire                                               BREADY            ,
    input    wire  [C_S_AXI_ADDR_WIDTH - 1:0]                   ARADDR            ,
    input    wire                                               ARVALID           ,
    output   wire                                               ARREADY           ,
    output   wire  [C_S_AXI_DATA_WIDTH - 1:0]                   RDATA             ,
    output   wire  [1:0]                                        RRESP             ,
    output   wire                                               RVALID            ,
    input    wire                                               RREADY            ,
    // Kernel control signals
    output   wire                                               ap_start          ,
    input    wire                                               ap_done           ,
    input    wire                                               ap_ready          ,
    input    wire                                               ap_idle           ,
    output   wire                                               interrupt         ,
    // User defined signals
    output   wire                                               user_rst_o        ,
    output   wire  [47 : 0]                                     local_mac_o       ,
    output   wire  [C_S_AXI_DATA_WIDTH-1 : 0]                   gateway_ip_o      ,
    output   wire  [C_S_AXI_DATA_WIDTH-1 : 0]                   subnet_mask_o     ,
    output   wire  [C_S_AXI_DATA_WIDTH-1 : 0]                   local_ip_o        ,
    output   wire  [C_S_AXI_DATA_WIDTH-1 : 0]                   udp_port_range_l_o,
    output   wire  [C_S_AXI_DATA_WIDTH-1 : 0]                   udp_port_range_h_o,
    output   wire  [C_S_AXI_DATA_WIDTH-1 : 0]                   shared_mem_o      ,

    inout    wire  [C_S_AXI_DATA_WIDTH*C_MAX_UDP_PORTS  -1 : 0] buffer_rx_vector_io, // MAX_UDP_PORTS sections (one per buffer), each containing 32 bits 
                                                                                    // {head[C_BUFFRX_INDEX_WIDTH], tail[C_BUFFRX_INDEX_WIDTH], empty, full, pushed, popped}
    input    wire                               bufrx_push_irq_i ,

    input    wire  [C_BUFFTX_INDEX_WIDTH-1 : 0] buftx_head_i    ,
    input    wire  [C_BUFFTX_INDEX_WIDTH-1 : 0] buftx_tail_i    ,
    input    wire  [                    -1 : 0] buftx_empty_i   ,
    input    wire  [                    -1 : 0] buftx_full_i    ,
    output   wire  [                    -1 : 0] buftx_pushed_o  ,
    input    wire  [                    -1 : 0] buftx_popped_i   
);

localparam ADDR_AP_CTRL_0_N_P        = 32'h00000000;  // ctrl_0 N_P Control Register Reserved
localparam ADDR_RES_0_Y_O            = 32'h00000008;  // user_rst_o_0 Y_O User Reset Output
localparam ADDR_MAC_0_N_O            = 32'h00000010;  // local_mac_o_0 N_O MAC Address Output
localparam ADDR_MAC_1_N_O            = 32'h00000018;  // local_mac_o_1 N_O MAC Address Output
localparam ADDR_GW_0_N_O             = 32'h00000020;  // gateway_ip_o_0 N_O Gateway Address Output
localparam ADDR_SNM_0_N_O            = 32'h00000028;  // subnet_mask_o_0 N_O Subnet Mask Output
localparam ADDR_IP_LOC_0_N_O         = 32'h00000030;  // local_ip_o_0 N_O Local IP Address Output
localparam ADDR_UDP_RANGE_L_0_N_O    = 32'h00000038;  // udp_port_range_l_o_0 N_O IP Address Listened Range Low Limit Output
localparam ADDR_UDP_RANGE_H_0_N_O    = 32'h00000040;  // udp_port_range_h_o_0 N_O IP Address Listened Range High Limit Output
localparam ADDR_SHMEM_0_N_O          = 32'h00000048;  // shared_mem_o_0 N_O Shared Memory Base Address Output
localparam ADDR_ISR0                 = 32'h00000050;  // Interrupt Set Register 0
localparam ADDR_IER0                 = 32'h00000058;  // Interrupt Enable Register 0
localparam ADDR_GIE                  = 32'h00000060;  // Interrupt Global Enable Register
localparam ADDR_BUFTX_HEAD_0_N_I     = 32'h00000068;  // buftx_head_i_0 N_I Buffer Tx Head
localparam ADDR_BUFTX_TAIL_0_N_I     = 32'h00000070;  // buftx_tail_i_0 N_I Buffer Tx Tail
localparam ADDR_BUFTX_EMPTY_0_N_I    = 32'h00000078;  // buftx_empty_i_0 N_I Buffer Tx Empty
localparam ADDR_BUFTX_FULL_0_N_I     = 32'h00000080;  // buftx_full_i_0 N_I Buffer Tx Full
localparam ADDR_BUFTX_PUSHED_0_Y_O   = 32'h00000088;  // buftx_pushed_o_0 Y_O Buffer Tx Pushed
localparam ADDR_BUFTX_POPPED_0_N_I   = 32'h00000090;  // buftx_popped_i_0 N_I Buffer Tx Popped
localparam ADDR_BUFRX_PUSH_IRQ_0_IRQ = 32'h00000098;  // bufrx_push_irq_i_0 IRQ Buffer Rx Irq
localparam ADDR_BUFRX_OFFSET_0_N_I   = 32'h000000a0;  // bufrx control regs take from this address to this address + (C_MAX_UDP_PORTS-1)+*8

/**********************************************************************************
* buffer rx vector handling
**********************************************************************************/

// buffer_rx_arr that wraps buffer_rx_vector_io as an array for ease its usage
wire [C_S_AXI_DATA_WIDTH-1:0] buffer_rx_arr [C_MAX_UDP_PORTS-1:0];
genvar buffer_rx_arr_index;
generate
    for (buffer_rx_arr_index = 0; buffer_rx_arr_index < C_MAX_UDP_PORTS; buffer_rx_arr_index = buffer_rx_arr_index + 1) begin
        // outputs (from array to vector)
        assign buffer_rx_vector_io[C_S_AXI_DATA_WIDTH*buffer_rx_arr_index + BUFFER_POPPED_OFFSET]   = buffer_rx_arr[buffer_rx_arr_index][BUFFER_POPPED_OFFSET];
        assign buffer_rx_vector_io[C_S_AXI_DATA_WIDTH*buffer_rx_arr_index + BUFFER_OPENSOCK_OFFSET] = buffer_rx_arr[buffer_rx_arr_index][BUFFER_OPENSOCK_OFFSET];
        // inputs (from vector to array)
        assign buffer_rx_arr[buffer_rx_arr_index][BUFFER_PUSHED_OFFSET]                    = buffer_rx_vector_io[C_S_AXI_DATA_WIDTH*buffer_rx_arr_index + BUFFER_PUSHED_OFFSET];
        assign buffer_rx_arr[buffer_rx_arr_index][BUFFER_FULL_OFFSET ]                     = buffer_rx_vector_io[C_S_AXI_DATA_WIDTH*buffer_rx_arr_index + BUFFER_FULL_OFFSET ];
        assign buffer_rx_arr[buffer_rx_arr_index][BUFFER_EMPTY_OFFSET]                     = buffer_rx_vector_io[C_S_AXI_DATA_WIDTH*buffer_rx_arr_index + BUFFER_EMPTY_OFFSET];
        assign buffer_rx_arr[buffer_rx_arr_index][BUFFER_TAIL_UPPER  : BUFFER_TAIL_OFFSET] = buffer_rx_vector_io[C_S_AXI_DATA_WIDTH*buffer_rx_arr_index + BUFFER_TAIL_UPPER : C_S_AXI_DATA_WIDTH*buffer_rx_arr_index + BUFFER_TAIL_OFFSET];
        assign buffer_rx_arr[buffer_rx_arr_index][BUFFER_HEAD_UPPER  : BUFFER_HEAD_OFFSET] = buffer_rx_vector_io[C_S_AXI_DATA_WIDTH*buffer_rx_arr_index + BUFFER_HEAD_UPPER : C_S_AXI_DATA_WIDTH*buffer_rx_arr_index + BUFFER_HEAD_OFFSET];
        assign buffer_rx_arr[buffer_rx_arr_index][BUFFER_DUMMY_UPPER : BUFFER_DUMMY_OFFSET] = 0;
    end
endgenerate

wire [C_MAX_UDP_PORTS-1:0] buffer_rx_arr_popped;
wire [C_MAX_UDP_PORTS-1:0] buffer_rx_arr_sockopen;
genvar buffer_rx_arr_outputs_index;
generate
    for (buffer_rx_arr_outputs_index = 0; buffer_rx_arr_outputs_index < C_MAX_UDP_PORTS; buffer_rx_arr_outputs_index = buffer_rx_arr_outputs_index + 1) begin
        assign buffer_rx_arr[buffer_rx_arr_outputs_index][BUFFER_POPPED_OFFSET] = buffer_rx_arr_popped[buffer_rx_arr_outputs_index];
        assign buffer_rx_arr[buffer_rx_arr_outputs_index][BUFFER_OPENSOCK_OFFSET] = buffer_rx_arr_sockopen[buffer_rx_arr_outputs_index];
    end
endgenerate

/**********************************************************************************
* FSM and states
**********************************************************************************/

// axi states for the write and read state machines
localparam
    WRIDLE    = 2'd0,
    WRDATA    = 2'd1,
    WRRESP    = 2'd2,
    WRRESET   = 2'd3,
    RDIDLE    = 2'd0,
    RDDATA    = 2'd1,
    RDRESET   = 2'd2;

// variables for write state machine
reg  [1:0]                    wstate;
reg  [1:0]                    wnext;

reg  [C_S_AXI_ADDR_WIDTH-1:0] waddr;
wire [C_S_AXI_DATA_WIDTH-1:0] wmask;
wire                            aw_hs;
wire                            w_hs;

// variables for read state machine
reg  [1:0]                    rstate = RDRESET;
reg  [1:0]                    rnext;
reg  [C_S_AXI_DATA_WIDTH-1:0] rdata;
wire                            ar_hs;
wire [C_S_AXI_ADDR_WIDTH-1:0] raddr;

/**********************************************************************************
* Register declarations and assignment to outputs
**********************************************************************************/

// kernel control registers
reg                             int_ap_idle= 1'b0;
reg                             int_ap_ready= 1'b0;
reg                             int_ap_done = 1'b0;
reg                             int_ap_start = 1'b0;
reg                             int_auto_restart = 1'b0;
// User's registers
reg                            user_rst_o_r         ; // User Reset Output
reg [47 : 0]                   local_mac_o_r        ; // MAC Address Output
reg [C_S_AXI_DATA_WIDTH-1 : 0] gateway_ip_o_r       ; // Gateway Address Output
reg [C_S_AXI_DATA_WIDTH-1 : 0] subnet_mask_o_r      ; // Subnet Mask Output
reg [C_S_AXI_DATA_WIDTH-1 : 0] local_ip_o_r         ; // Local IP Address Output
reg [C_S_AXI_DATA_WIDTH-1 : 0] udp_port_range_l_o_r ; // IP Address Listened Range Low Limit Output
reg [C_S_AXI_DATA_WIDTH-1 : 0] udp_port_range_h_o_r ; // IP Address Listened Range High Limit Output
reg [C_S_AXI_DATA_WIDTH-1 : 0] shared_mem_o_r       ; // Shared Memory Base Address Output
reg [C_S_AXI_DATA_WIDTH-1 : 0] bufrx_temp_arr_r [C_MAX_UDP_PORTS-1 : 0];
reg                            buftx_pushed_o_r     ; // Buffer Tx Pushed
// End of user's registers

// Internal IRQ registers
reg int_gie;
reg [C_S_AXI_DATA_WIDTH-1 : 0] ext_isr0;
reg [C_S_AXI_DATA_WIDTH-1 : 0] ext_ier0;
// end of internal IRQ registers

assign user_rst_o           = user_rst_o_r          ; // User Reset Output
assign local_mac_o          = local_mac_o_r         ; // MAC Address Output                                
assign gateway_ip_o         = gateway_ip_o_r        ; // Gateway Address Output                            
assign subnet_mask_o        = subnet_mask_o_r       ; // Subnet Mask Output                                
assign local_ip_o           = local_ip_o_r          ; // Local IP Address Output                           
assign udp_port_range_l_o   = udp_port_range_l_o_r  ; // IP Address Listened Range Low Limit Output        
assign udp_port_range_h_o   = udp_port_range_h_o_r  ; // IP Address Listened Range High Limit Output       
assign shared_mem_o         = shared_mem_o_r        ; // Shared Memory Base Address Output        

genvar bufrx_outputs_r_index;
generate
    for (bufrx_outputs_r_index = 0; bufrx_outputs_r_index < C_MAX_UDP_PORTS; bufrx_outputs_r_index = bufrx_outputs_r_index + 1) begin
        assign buffer_rx_arr_popped[bufrx_outputs_r_index]   = bufrx_temp_arr_r[bufrx_outputs_r_index][BUFFER_POPPED_OFFSET];
        assign buffer_rx_arr_sockopen[bufrx_outputs_r_index] = bufrx_temp_arr_r[bufrx_outputs_r_index][BUFFER_OPENSOCK_OFFSET];
    end    
endgenerate

assign buftx_pushed_o = buftx_pushed_o_r ; // Buffer Tx Pushed   
                                                                                                  
/**********************************************************************************
* AXI write fsm
**********************************************************************************/

assign AWREADY = (wstate == WRIDLE);
assign WREADY  = (wstate == WRDATA);
assign BRESP   = 2'b00;  // OKAY
assign BVALID  = (wstate == WRRESP);
assign wmask   = { {8{WSTRB[3]}}, {8{WSTRB[2]}}, {8{WSTRB[1]}}, {8{WSTRB[0]}} };
assign aw_hs   = AWVALID & AWREADY;
assign w_hs    = WVALID & WREADY;

// wstate
always @(posedge clk) begin
    if (!res_n) wstate <= WRRESET;
    else        wstate <= wnext;
end

// wnext
always @(*) begin
    wnext = wstate;
    case (wstate)
        WRIDLE: if (AWVALID) wnext = WRDATA;
        WRDATA: if (WVALID)  wnext = WRRESP;
        WRRESP: if (BREADY)  wnext = WRIDLE;
        default:             wnext = WRIDLE;
    endcase
end

// waddr
always @(posedge clk) begin
    if (aw_hs) waddr <= AWADDR[C_S_AXI_ADDR_WIDTH-1:0];
end

/**********************************************************************************
* AXI read fsm
**********************************************************************************/

assign ARREADY = (rstate == RDIDLE);
assign RDATA   = rdata;
assign RRESP   = 2'b00;  // OKAY
assign RVALID  = (rstate == RDDATA);
assign ar_hs   = ARVALID & ARREADY;
assign raddr   = ARADDR[C_S_AXI_ADDR_WIDTH-1:0];

// rstate
always @(posedge clk) begin
    if (!res_n) rstate <= RDRESET;
    else        rstate <= rnext;
end

// rnext
always @(*) begin
    rnext = rstate;
    case (rstate) 
        RDIDLE: if (ARVALID)         rnext = RDDATA;
        RDDATA: if (RREADY & RVALID) rnext = RDIDLE;
        default:                     rnext = RDIDLE;
    endcase
end

// rdata
always @(posedge clk) begin

    if (ar_hs) begin

        rdata <= 'b0;

        if (raddr < ADDR_BUFRX_OFFSET_0_N_I) begin

            case (raddr)

            // Control signal as per Kernel definition

            ADDR_AP_CTRL_0_N_P: begin
                rdata[0] <= int_ap_start;
                rdata[1] <= int_ap_done;
                rdata[2] <= int_ap_idle;
                rdata[3] <= int_ap_ready;
                rdata[7] <= int_auto_restart;
            end

            // IRQ Management registers

            ADDR_ISR0 : begin rdata <= ext_isr0; end
            ADDR_IER0 : begin rdata <= ext_ier0; end
            ADDR_GIE  : begin rdata <= int_gie;  end

            // Application registers

            ADDR_RES_0_Y_O              : rdata <=  user_rst_o_r;
            ADDR_MAC_0_N_O              : rdata <=  local_mac_o_r[C_S_AXI_DATA_WIDTH - 1 : 0];
            ADDR_MAC_1_N_O              : rdata <=  {{16{1'b0}},local_mac_o_r[(C_S_AXI_DATA_WIDTH * 2) - 17 : C_S_AXI_DATA_WIDTH]};
            ADDR_GW_0_N_O               : rdata <=  gateway_ip_o_r[C_S_AXI_DATA_WIDTH - 1 : 0];
            ADDR_SNM_0_N_O              : rdata <=  subnet_mask_o_r[C_S_AXI_DATA_WIDTH - 1 : 0];
            ADDR_IP_LOC_0_N_O           : rdata <=  local_ip_o_r[C_S_AXI_DATA_WIDTH - 1 : 0];
            ADDR_UDP_RANGE_L_0_N_O      : rdata <=  udp_port_range_l_o_r[C_S_AXI_DATA_WIDTH - 1 : 0];
            ADDR_UDP_RANGE_H_0_N_O      : rdata <=  udp_port_range_h_o_r[C_S_AXI_DATA_WIDTH - 1 : 0];
            ADDR_SHMEM_0_N_O            : rdata <=  shared_mem_o_r[C_S_AXI_DATA_WIDTH - 1 : 0];
            ADDR_BUFTX_HEAD_0_N_I       : rdata <=  buftx_head_i;
            ADDR_BUFTX_TAIL_0_N_I       : rdata <=  buftx_tail_i;
            ADDR_BUFTX_EMPTY_0_N_I      : rdata <=  buftx_empty_i;
            ADDR_BUFTX_FULL_0_N_I       : rdata <=  buftx_full_i;
            ADDR_BUFTX_PUSHED_0_Y_O     : rdata <=  buftx_pushed_o_r;
            ADDR_BUFTX_POPPED_0_N_I     : rdata <=  buftx_popped_i;
            
            default                     : rdata <= 32'hDEADBEEF;
            endcase

        end else if (raddr < ADDR_BUFRX_OFFSET_0_N_I + 8 * C_MAX_UDP_PORTS ) begin
            rdata <= buffer_rx_arr[(raddr-ADDR_BUFRX_OFFSET_0_N_I)/8];
        end else begin
            rdata <= 32'hDEADBEEF;
        end

   end
end

/**********************************************************************************
* Output registers handling
**********************************************************************************/

assign ap_start  = int_ap_start;
reg [log2(C_MAX_UDP_PORTS) : 0] bufrx_temp_index;

always @(posedge clk) begin
    if (!res_n) begin
        user_rst_o_r          <= 0;
        local_mac_o_r         <= 0;
        gateway_ip_o_r        <= 0;
        subnet_mask_o_r       <= 0;
        local_ip_o_r          <= 0;
        udp_port_range_l_o_r  <= 0;
        udp_port_range_h_o_r  <= 0;
        shared_mem_o_r        <= 0;
        ext_ier0              <= 0;
        for (bufrx_temp_index = 0; bufrx_temp_index < C_MAX_UDP_PORTS; bufrx_temp_index = bufrx_temp_index + 1) bufrx_temp_arr_r[bufrx_temp_index] <= 0;
        buftx_pushed_o_r      <= 0;

    end
    if (w_hs) begin
        if (waddr < ADDR_BUFRX_OFFSET_0_N_I) begin
            case (waddr)
            ADDR_RES_0_Y_O          : user_rst_o_r                                                      <= (WDATA[C_S_AXI_DATA_WIDTH-1:0] & wmask) | (user_rst_o_r & ~wmask);
            ADDR_MAC_0_N_O          : local_mac_o_r[C_S_AXI_DATA_WIDTH - 1 : 0]                         <= (WDATA[C_S_AXI_DATA_WIDTH-1:0] & wmask) | (local_mac_o_r[C_S_AXI_DATA_WIDTH - 1 : 0] & ~wmask);
            ADDR_MAC_1_N_O          : local_mac_o_r[(C_S_AXI_DATA_WIDTH * 2) - 17 : C_S_AXI_DATA_WIDTH] <= (WDATA[C_S_AXI_DATA_WIDTH-1:0] & wmask) | (local_mac_o_r[(C_S_AXI_DATA_WIDTH * 2) - 17 : C_S_AXI_DATA_WIDTH] & ~wmask);
            ADDR_GW_0_N_O           : gateway_ip_o_r[C_S_AXI_DATA_WIDTH - 1 : 0]                        <= (WDATA[C_S_AXI_DATA_WIDTH-1:0] & wmask) | (gateway_ip_o_r[C_S_AXI_DATA_WIDTH - 1 : 0] & ~wmask);
            ADDR_SNM_0_N_O          : subnet_mask_o_r[C_S_AXI_DATA_WIDTH - 1 : 0]                       <= (WDATA[C_S_AXI_DATA_WIDTH-1:0] & wmask) | (subnet_mask_o_r[C_S_AXI_DATA_WIDTH - 1 : 0] & ~wmask);
            ADDR_IP_LOC_0_N_O       : local_ip_o_r[C_S_AXI_DATA_WIDTH - 1 : 0]                          <= (WDATA[C_S_AXI_DATA_WIDTH-1:0] & wmask) | (local_ip_o_r[C_S_AXI_DATA_WIDTH - 1 : 0] & ~wmask);
            ADDR_UDP_RANGE_L_0_N_O  : udp_port_range_l_o_r[C_S_AXI_DATA_WIDTH - 1 : 0]                  <= (WDATA[C_S_AXI_DATA_WIDTH-1:0] & wmask) | (udp_port_range_l_o_r[C_S_AXI_DATA_WIDTH - 1 : 0] & ~wmask);
            ADDR_UDP_RANGE_H_0_N_O  : udp_port_range_h_o_r[C_S_AXI_DATA_WIDTH - 1 : 0]                  <= (WDATA[C_S_AXI_DATA_WIDTH-1:0] & wmask) | (udp_port_range_h_o_r[C_S_AXI_DATA_WIDTH - 1 : 0] & ~wmask);
            ADDR_SHMEM_0_N_O        : shared_mem_o_r[C_S_AXI_DATA_WIDTH - 1 : 0]                        <= (WDATA[C_S_AXI_DATA_WIDTH-1:0] & wmask) | (shared_mem_o_r[C_S_AXI_DATA_WIDTH - 1 : 0] & ~wmask);
            ADDR_IER0               : ext_ier0[C_S_AXI_DATA_WIDTH - 1 : 0]                              <= (WDATA[C_S_AXI_DATA_WIDTH-1:0] & wmask) | (ext_ier0[C_S_AXI_DATA_WIDTH - 1 : 0] & ~wmask);
            ADDR_BUFTX_PUSHED_0_Y_O : buftx_pushed_o_r                                                  <= (WDATA[C_S_AXI_DATA_WIDTH-1:0] & wmask) | ({ buftx_pushed_o_r} & ~wmask);
            endcase

        end else if (waddr < ADDR_BUFRX_OFFSET_0_N_I + 8 * C_MAX_UDP_PORTS) begin
            bufrx_temp_arr_r[(waddr-ADDR_BUFRX_OFFSET_0_N_I)/8] <= ( WDATA[C_S_AXI_DATA_WIDTH-1:0] & wmask ) | ( bufrx_temp_arr_r[(waddr-ADDR_BUFRX_OFFSET_0_N_I)/8] & ~wmask );
        end

    end
end

/**********************************************************************************
* Kernel control signal management
**********************************************************************************/

// int_ap_start
always @(posedge clk) begin
    if (!res_n)                                                                 int_ap_start <= 1'b0;
    else begin if (w_hs && waddr == ADDR_AP_CTRL_0_N_P && WSTRB[0] && WDATA[0]) int_ap_start <= 1'b1;
          else if (ap_ready)                                                    int_ap_start <= int_auto_restart; // clear on handshake/auto restart
    end
end

// int_ap_done
always @(posedge clk) begin
    if (!res_n)                                     int_ap_done <= 1'b0;
    else if (ap_done)                               int_ap_done <= 1'b1;
    else if (ar_hs && raddr == ADDR_AP_CTRL_0_N_P)  int_ap_done <= 1'b0; // clear on read
end

// int_ap_idle
always @(posedge clk) begin
    if (!res_n) int_ap_idle <= 1'b0;
    else        int_ap_idle <= ap_idle;
end

// int_ap_ready
always @(posedge clk) begin
    if (!res_n) int_ap_ready <= 1'b0;
    else        int_ap_ready <= ap_ready;
end

// int_auto_restart
always @(posedge clk) begin
    if (!res_n)                                                 int_auto_restart <= 1'b0;
    else if (w_hs && waddr == ADDR_AP_CTRL_0_N_P && WSTRB[0])   int_auto_restart <=  WDATA[7];
end

reg ext_isr0_mutex;
always @(posedge clk) begin
    if (!res_n) begin
        ext_isr0 <= 0;
        ext_isr0_mutex <= 1'b1;
    end else begin 
        if (w_hs && waddr == ADDR_ISR0) begin
            ext_isr0[0] <= ext_isr0[0] ^ ext_isr0_mutex; // toggle on write
            ext_isr0_mutex <= 1'b0;
        end else begin
            ext_isr0[0] <= ext_ier0[0] & bufrx_push_irq_i | ext_isr0[0];
            ext_isr0_mutex <= 1'b1;
        end
    end
end

// Handle Global Interrupt Enable
assign interrupt = int_gie & (|{ ext_isr0 });

always @(posedge clk) begin
	if (!res_n)                                     int_gie <= 1'b0;
	else if (w_hs && waddr == ADDR_GIE && WSTRB[0]) int_gie <= WDATA[0];
end

// Handle bitfield pulse output


endmodule

