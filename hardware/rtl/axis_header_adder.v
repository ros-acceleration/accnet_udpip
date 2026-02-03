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
 * axis_header_adder: includes the header at the beginning of the output stream,
 * forwarding then the main (payload) input stream
 * Behavior:
    1) IDLE: wait for hdr_valid
    2) HEADER: add the N header words at m_axis, once per cycle, in the following order:
    {udp_length, source_ip, source_port, dest_ip, dest_port}
    3) PAYLOAD: forward the payload
**********************************************************************************/

module axis_header_adder #(
    parameter HEADER_NUM_WORDS  = 5
)(

    input  wire clk ,
    input  wire rst ,

    output reg          hdr_ready          ,
    input  wire         hdr_valid          ,
    input  wire         hdr_valid_port     ,
    input  wire [31:00] hdr_source_ip      ,
    input  wire [15:00] hdr_source_port    ,
    input  wire [31:00] hdr_dest_ip        ,
    input  wire [15:00] hdr_dest_port      ,
    input  wire [15:00] hdr_udp_length     ,

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

// Counter to wait for HEADER_NUM_WORDS at STATE_FORW_HDR state to forward every header word

reg [log2(HEADER_NUM_WORDS):0] count_header;
always @ (posedge clk) begin
    if      (rst                                                                      ) count_header <= 0;
    else if (state == STATE_FORW_HDR && axis_forwarded_tready && axis_forwarded_tvalid) count_header <= count_header + 1;
    else if (state != STATE_FORW_HDR                                                  ) count_header <= 0;
end

// FSM

localparam
    STATE_IDLE     = 2'd0,
    STATE_FORW_HDR = 2'd1,
    STATE_FORW_PYL = 2'd2;
reg [1:0] state = STATE_IDLE;

wire payload_last;
assign payload_last = s_axis_payload_tready & s_axis_payload_tvalid & s_axis_payload_tlast;

always @ (posedge clk) begin
    if (rst) 
        state <= STATE_IDLE;
    else begin
        case (state)
            STATE_IDLE      : if (hdr_ready & hdr_valid_port                                                          ) state <= STATE_FORW_HDR;
            STATE_FORW_HDR  : if (count_header == HEADER_NUM_WORDS-1 && axis_forwarded_tready && axis_forwarded_tvalid) state <= STATE_FORW_PYL;
            STATE_FORW_PYL  : if (payload_last                                                                        ) state <= STATE_IDLE;
            default         :                                                                                           state <= STATE_IDLE;
        endcase
    end
end

// Data to be forwarded

localparam UDP_OVERHEAD_LENGTH = 8; // hdr_udp_length counts 8 extra bytes (for metadata) that are not part of the payload

always @ (*) begin
    case (state)

        STATE_FORW_HDR: begin
            hdr_ready             <= 1'b0;
            s_axis_payload_tready <= 1'b0;
            axis_forwarded_tkeep  <= {8{1'b1}};
            axis_forwarded_tvalid <= 1'b1;
            axis_forwarded_tlast  <= 1'b0;
            axis_forwarded_tuser  <= 1'b0;
            case (count_header)
                0      : axis_forwarded_tdata <= {48'b0, hdr_udp_length-UDP_OVERHEAD_LENGTH};
                1      : axis_forwarded_tdata <= {32'b0, hdr_source_ip};
                2      : axis_forwarded_tdata <= {48'b0, hdr_source_port};
                3      : axis_forwarded_tdata <= {32'b0, hdr_dest_ip};
                4      : axis_forwarded_tdata <= {48'b0, hdr_dest_port};
                default: axis_forwarded_tdata <= {64'b0};
            endcase
        end

        STATE_FORW_PYL: begin
            hdr_ready             <= 1'b0;
            s_axis_payload_tready <= axis_forwarded_tready;
            axis_forwarded_tdata  <= s_axis_payload_tdata ;
            axis_forwarded_tkeep  <= s_axis_payload_tkeep ;
            axis_forwarded_tvalid <= s_axis_payload_tvalid;
            axis_forwarded_tlast  <= s_axis_payload_tlast ;
            axis_forwarded_tuser  <= s_axis_payload_tuser ;
        end

        default: begin
            hdr_ready             <= axis_forwarded_tready;
            s_axis_payload_tready <= 1'b0;
            axis_forwarded_tdata  <= {64{1'bx}};
            axis_forwarded_tkeep  <= {8{1'bx}};
            axis_forwarded_tvalid <= 1'b0;
            axis_forwarded_tlast  <= 1'bx;
            axis_forwarded_tuser  <= 1'bx;
        end

    endcase
end

// Register output

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