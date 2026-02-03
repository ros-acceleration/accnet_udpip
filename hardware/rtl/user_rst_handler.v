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
 * circular user_rst_handler:
 *   - Receives a user reset request
 *   - Asserts the reset at the output only when the system is not busy
 *   - Each time a POR is received, enable goes low until a user reset is received. This
 *   ensure that rst_user_o keeps high until the user has finished the configuration and
 *   asserts rst_user_i
**********************************************************************************/

module user_rst_handler (
    input  wire clk_i       ,
    input  wire rst_user_i  ,
    input  wire rst_por_i   ,
    input  wire rx_busy_i   ,
    input  wire tx_busy_i   ,
    output reg  rst_user_o  
 );

/**********************************************************************************
 * Main logic
 **********************************************************************************/

reg rst_requested;
always @ (posedge clk_i) begin
    if      (rst_por_i  ) rst_requested = 0;
    else if (rst_user_i ) rst_requested = 1;
    else if (rst_user_o ) rst_requested = 0;
end

wire busy;
assign busy = rx_busy_i || tx_busy_i;

reg enable = 0;
always @ (posedge clk_i) begin
    if      (rst_por_i    ) enable = 0;
    else if (rst_requested) enable = 1;
end

always @ (posedge clk_i) begin
    if      (rst_por_i) rst_user_o = 1;
    else if (!enable  ) rst_user_o = 1;
    else                rst_user_o = rst_requested && !busy;
end

endmodule