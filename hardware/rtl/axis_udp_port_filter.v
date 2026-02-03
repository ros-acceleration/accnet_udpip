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
 * axis_udp_port_filter: for an incoming packet, it checks if the destination port
 * matches any open socket (within the udp port range), forwards the packet if so
 * or discards the packet otherwise
 * Behavior:
    1) IDLE: waits for hdr_valid and dma_done
    2) CHECK_PORT: checks if destination port is in udp port range and if its socket
        is open. If so, goes to 3. Otherwise, goes to 4
    3) FORWARD: forwards the packet. Goes to 1
    4) DISCARD: discards the packet. Goes to 1
**********************************************************************************/

module axis_udp_port_filter #(

    parameter MAX_UDP_PORTS        = 1024

)(

    input  wire clk ,
    input  wire rst ,

    input  wire                             hdr_valid           ,
    input  wire [15:00                    ] hdr_dest_port       ,
    input  wire                             dma_done_i          ,
    input  wire [15:00                    ] udp_port_range_lower,
    input  wire [15:00                    ] udp_port_range_upper,
    input  wire [MAX_UDP_PORTS-1 : 0      ] open_sockets_vector ,
    output reg  [log2(MAX_UDP_PORTS)-1 : 0] buffer_select_idx_o ,
    output reg                              valid_udp_port_o    ,

    output reg          s_axis_payload_tready,
    input  wire         s_axis_payload_tvalid,
    input  wire [63:00] s_axis_payload_tdata ,
    input  wire [07:00] s_axis_payload_tkeep ,
    input  wire         s_axis_payload_tlast ,
    input  wire         s_axis_payload_tuser ,    

    input  wire         m_axis_tready,
    output wire         m_axis_tvalid,
    output wire [63:00] m_axis_tdata ,
    output wire [07:00] m_axis_tkeep ,
    output wire         m_axis_tlast ,
    output wire         m_axis_tuser 

);

/**********************************************************************************
 * Main logic
**********************************************************************************/

wire         axis_forwarded_tready;
reg          axis_forwarded_tvalid;
reg  [63:00] axis_forwarded_tdata ;
reg  [07:00] axis_forwarded_tkeep ;
reg          axis_forwarded_tlast ;
reg          axis_forwarded_tuser ;

// FSM

localparam
    STATE_IDLE       = 2'd0,
    STATE_CHECK_PORT = 2'd1,
    STATE_FORWARD    = 2'd2,
    STATE_DISCARD    = 2'd3;
reg [1:0] state = STATE_IDLE;

wire payload_last;
assign payload_last = s_axis_payload_tready & s_axis_payload_tvalid & s_axis_payload_tlast;

wire [log2(MAX_UDP_PORTS)-1 : 0] buffer_offset_for_current_port;
assign buffer_offset_for_current_port = hdr_dest_port - udp_port_range_lower;
wire socket_is_open;
assign socket_is_open = open_sockets_vector[buffer_offset_for_current_port];
wire valid_udp_port;
assign valid_udp_port = (hdr_dest_port >= udp_port_range_lower && hdr_dest_port <= udp_port_range_upper && socket_is_open);

always @ (posedge clk) begin
    if (rst) 
        state <= STATE_IDLE;
    else begin
        valid_udp_port_o <= valid_udp_port && hdr_valid;
        case (state)
            STATE_IDLE       : if (hdr_valid                ) state <= STATE_CHECK_PORT;
            STATE_CHECK_PORT : if (valid_udp_port           ) state <= STATE_FORWARD;
                               else                           state <= STATE_DISCARD;
            STATE_FORWARD    : if (payload_last             ) state <= STATE_IDLE;
            STATE_DISCARD    : if (payload_last             ) state <= STATE_IDLE;
            default          :                                state <= STATE_IDLE;
        endcase
    end
end

// Data to be forwarded

always @ (*) begin
    axis_forwarded_tdata <= s_axis_payload_tdata ;
    axis_forwarded_tkeep <= s_axis_payload_tkeep ;
    axis_forwarded_tlast <= s_axis_payload_tlast ;
    axis_forwarded_tuser <= s_axis_payload_tuser ;

    case (state)

        STATE_FORWARD: begin
            s_axis_payload_tready <= axis_forwarded_tready;
            axis_forwarded_tvalid <= s_axis_payload_tvalid;
        end

        STATE_CHECK_PORT: begin
            s_axis_payload_tready <= 1'b0;
            axis_forwarded_tvalid <= 1'b0;
        end

        STATE_DISCARD: begin
            s_axis_payload_tready <= 1'b1;
            axis_forwarded_tvalid <= 1'b0;
        end

        default: begin
            s_axis_payload_tready <= axis_forwarded_tready;
            axis_forwarded_tvalid <= 1'b0;
        end

    endcase
end

// Register output

always @ (posedge clk) begin
    if      (rst                      ) buffer_select_idx_o <= 0;
    else if (state == STATE_CHECK_PORT) buffer_select_idx_o <= buffer_offset_for_current_port;
end

axis_register #(
    .DATA_WIDTH      (64 ),
    .KEEP_ENABLE     (1  ),
    .LAST_ENABLE     (1  ),
    .ID_ENABLE       (0  ),
    .DEST_ENABLE     (0  ),
    .USER_ENABLE     (1  ),
    .USER_WIDTH      (1  ),
    .REG_TYPE        (2  )
) axis_register_inst (
    .clk              (clk ),
    .rst              (rst ),
    .s_axis_tdata     (axis_forwarded_tdata  ),
    .s_axis_tkeep     (axis_forwarded_tkeep  ),
    .s_axis_tvalid    (axis_forwarded_tvalid ),
    .s_axis_tready    (axis_forwarded_tready ),
    .s_axis_tlast     (axis_forwarded_tlast  ),
    .s_axis_tuser     (axis_forwarded_tuser  ),
    .m_axis_tdata     (m_axis_tdata  ),
    .m_axis_tkeep     (m_axis_tkeep  ),
    .m_axis_tvalid    (m_axis_tvalid ),
    .m_axis_tready    (m_axis_tready ),
    .m_axis_tlast     (m_axis_tlast  ),
    .m_axis_tuser     (m_axis_tuser  )
);

endmodule