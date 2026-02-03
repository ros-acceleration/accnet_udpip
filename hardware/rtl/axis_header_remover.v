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
 * axis_header_remover: extracts the header from the incoming axi stream,
 * forwarding the rest (payload) to the main output stream
 * Behavior:
    1) IDLE: wait for valid
    2) HEADER: extracts the N header words at s_axis, one per cycle, in the following order:
    {udp_length, source_ip, source_port, dest_ip, dest_port}
    3) PAYLOAD: forward the payload to m_axis (only up to hdr_udp_length cycles; the rest
    of the incoming stream is not forwarded)
**********************************************************************************/

module axis_header_remover #(
    parameter HEADER_NUM_WORDS  = 5,
    parameter TRANS_MAX_LENGTH  = 2*1024
)(

    input  wire clk ,
    input  wire rst ,

    output reg          s_axis_tready,
    input  wire         s_axis_tvalid,
    input  wire [63:00] s_axis_tdata ,
    input  wire [07:00] s_axis_tkeep ,
    input  wire         s_axis_tlast ,
    input  wire         s_axis_tuser ,    

    input  wire         hdr_ready      ,
    output reg          hdr_valid      ,
    output reg  [31:00] hdr_source_ip  ,
    output reg  [15:00] hdr_source_port,
    output reg  [31:00] hdr_dest_ip    ,
    output reg  [15:00] hdr_dest_port  ,
    output reg  [15:00] hdr_udp_length ,

    input  wire         m_axis_tready,
    output wire         m_axis_tvalid,
    output wire [63:00] m_axis_tdata ,
    output wire [07:00] m_axis_tkeep ,
    output wire         m_axis_tlast ,
    output wire         m_axis_tuser 

);

localparam WORDS_PER_CYCLE = 64/8;

/**********************************************************************************
 * Main logic
**********************************************************************************/

wire         axis_forwarded_tready;
reg          axis_forwarded_tvalid;
wire [63:00] axis_forwarded_tdata ;
reg  [07:00] axis_forwarded_tkeep ;
wire         axis_forwarded_tlast ;
wire         axis_forwarded_tuser ;

// Counter to wait for HEADER_NUM_WORDS at STATE_HEADER state to forward every header word

wire s_axis_consumed;
assign s_axis_consumed = s_axis_tready & s_axis_tvalid;

reg [log2(HEADER_NUM_WORDS):0] count_header;
always @ (posedge clk) begin
    if      (rst                                     ) count_header <= 0;
    else if (state == STATE_HEADER && s_axis_consumed) count_header <= count_header + 1;
    else if (state != STATE_HEADER                   ) count_header <= 0;
end

// FSM

localparam
    STATE_HEADER  = 2'd0,
    STATE_PAYLOAD = 2'd1;
reg [1:0] state = STATE_HEADER;

always @ (posedge clk) begin
    if (rst) 
        state <= STATE_HEADER;
    else begin
        case (state)
            STATE_HEADER  : if (count_header == HEADER_NUM_WORDS-1 && s_axis_consumed) state <= STATE_PAYLOAD;
            STATE_PAYLOAD : if (s_axis_consumed && s_axis_tlast                      ) state <= STATE_HEADER;
            default       :                                                            state <= STATE_HEADER;
        endcase
    end
end

// Header extraction

always @ (posedge clk) begin
    if      (state == STATE_HEADER && count_header == HEADER_NUM_WORDS-1 && s_axis_consumed) hdr_valid <= 1'b1;
    else if (hdr_ready                                                                     ) hdr_valid <= 1'b0;
end

always @ (posedge clk) begin
    if (rst)                                             hdr_udp_length <= 0;
    else if (state == STATE_HEADER && count_header == 0) hdr_udp_length <= s_axis_tdata[15:00];
end

always @ (posedge clk) begin
    if (rst)                                             hdr_source_ip <= 0;
    else if (state == STATE_HEADER && count_header == 1) hdr_source_ip <= s_axis_tdata[31:00];
end

always @ (posedge clk) begin
    if (rst)                                             hdr_source_port <= 0;
    else if (state == STATE_HEADER && count_header == 2) hdr_source_port <= s_axis_tdata[15:00];
end

always @ (posedge clk) begin
    if (rst)                                             hdr_dest_ip <= 0;
    else if (state == STATE_HEADER && count_header == 3) hdr_dest_ip <= s_axis_tdata[31:00];
end

always @ (posedge clk) begin
    if (rst)                                             hdr_dest_port <= 0;
    else if (state == STATE_HEADER && count_header == 4) hdr_dest_port <= s_axis_tdata[15:00];
end

// Forwarded stream control

reg [log2(TRANS_MAX_LENGTH/WORDS_PER_CYCLE):0] transf_count;
always @ (posedge clk) begin
    if      (rst || state != STATE_PAYLOAD) transf_count <= 0;
    else if (s_axis_consumed)               transf_count <= transf_count+1;
end

reg transf_last;
always @ (*) begin
    if (state != STATE_PAYLOAD || transf_count*WORDS_PER_CYCLE < hdr_udp_length-WORDS_PER_CYCLE) transf_last <= 0;
    else                                                                                         transf_last <= 1;
end

reg transf_done;
always @ (*) begin
    if (state != STATE_PAYLOAD || transf_count*WORDS_PER_CYCLE < hdr_udp_length) transf_done <= 0;
    else                                                                         transf_done <= 1;
end

assign axis_forwarded_tdata = s_axis_tdata;
// assign axis_forwarded_tkeep = s_axis_tkeep;
always @ (*) begin
  if (state != STATE_PAYLOAD || transf_count <= hdr_udp_length/WORDS_PER_CYCLE-1) axis_forwarded_tkeep <= s_axis_tkeep;
  else                                                                            axis_forwarded_tkeep <= (1 << (hdr_udp_length - transf_count*WORDS_PER_CYCLE)) - 1;
end
assign axis_forwarded_tlast = s_axis_tlast || transf_last;
assign axis_forwarded_tuser = s_axis_tuser;
always @ (*) begin
    case (state)
        STATE_HEADER:  begin axis_forwarded_tvalid <= 1'b0; s_axis_tready <= axis_forwarded_tready; end
        STATE_PAYLOAD: begin axis_forwarded_tvalid <= s_axis_tvalid & !transf_done; s_axis_tready <= axis_forwarded_tready; end
        default:       begin axis_forwarded_tvalid <= 1'b0; s_axis_tready <= 1'b0; end
    endcase
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