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
 * pulse on posedge
 **********************************************************************************/

module pulse_on_posedge (
    input  wire         clk_i          ,
    input  wire         rst_i          ,
    input  wire         signal_rising_i,
    output wire         signal_pulse_o 
);

wire signal_rose;
reg signal_reg1, signal_reg2;
always @(posedge clk_i) begin
    if (rst_i) begin 
        signal_reg1 <= 0;
        signal_reg2 <= 0;
    end else begin
        signal_reg1 <= signal_rising_i;
        signal_reg2 <= signal_reg1;
    end
end
assign signal_pulse_o = signal_reg1 & !signal_reg2;

endmodule
