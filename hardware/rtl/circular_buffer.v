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
 * circular buffer:
 *   - Handles the control variables for a circular buffer (without allocating the buffer itself)
 *   - Updates head_index and full when data_pushed_i is active (must be a single pulse)
 *   - Updates tail_index and empty when data_popped_i is active (must be a single pulse)    
 **********************************************************************************/

module circular_buffer #(
    parameter BUFFER_LENGTH = 32,
    parameter INDEX_WIDTH = 5 // log2(BUFFER_LENGTH)
) (

    input  wire clk_i ,
    input  wire rst_i ,

    input  wire                      data_pushed_i ,
    input  wire                      data_popped_i ,
    output reg  [INDEX_WIDTH-1 : 00] head_index_o  ,
    output reg  [INDEX_WIDTH-1 : 00] tail_index_o  ,
    output reg                       full_o        ,
    output reg                       empty_o       
 );

/**********************************************************************************
 * Main logic
 **********************************************************************************/

// head index update

reg [INDEX_WIDTH-1 : 00] head_index_next;
always @ * begin
    if  (head_index_o < BUFFER_LENGTH-1) head_index_next = head_index_o + 1;
    else                                 head_index_next = 0;
end

always @ (posedge clk_i) begin
    if      (rst_i        ) head_index_o <= 0;
    else if (data_pushed_i) head_index_o <= head_index_next;
end

// full only can change if (1) popped but didn't push (won't be full) or (2) pushed but didn't pop (could get full)

always @ (posedge clk_i) begin
    if      (rst_i)                                 full_o <= 0;
    else if (!data_pushed_i && data_popped_i)       full_o <= 0;
    else if (data_pushed_i && !data_popped_i) begin
        if (head_index_next == tail_index_o)        full_o <= 1;
        else                                        full_o <= 0;
    end
end

// tail index update

reg [INDEX_WIDTH-1 : 00] tail_index_next;
always @ * begin
    if  (tail_index_o < BUFFER_LENGTH-1) tail_index_next = tail_index_o + 1;
    else                                 tail_index_next = 0;
end

always @ (posedge clk_i) begin
    if      (rst_i        ) tail_index_o <= 0;
    else if (data_popped_i) tail_index_o <= tail_index_next;
end

// empty only can change if (1) pushed but didn't pop (won't be empty) or (2) popped but didn't push (could get full)

always @ (posedge clk_i) begin
    if      (rst_i)                                 empty_o <= 1;
    else if (data_pushed_i && !data_popped_i)       empty_o <= 0;
    else if (data_popped_i && !data_pushed_i) begin
        if (tail_index_next == head_index_o)        empty_o <= 1;
        else                                        empty_o <= 0;
    end
end

endmodule