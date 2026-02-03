/*

Copyright (c) 2020-2021 Alex Forencich
Copyright (c) 2024 Gabriele Serra

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

/*
 * FPGA top-level module
 */
module fpga #
(
    // AXI interface configuration (DMA)
    parameter AXI_DATA_WIDTH = 128,
    parameter AXI_ADDR_WIDTH = 32,
    parameter AXI_STRB_WIDTH = (AXI_DATA_WIDTH/8),
    parameter AXI_ID_WIDTH = 8,

    // AXI lite interface configuration (control)
    parameter AXIL_CTRL_DATA_WIDTH = 32,
    parameter AXIL_CTRL_ADDR_WIDTH = 24,
    parameter AXIL_CTRL_STRB_WIDTH = (AXIL_CTRL_DATA_WIDTH/8),

    // AXI lite interface configuration (application control)
    parameter AXIL_APP_CTRL_DATA_WIDTH = AXIL_CTRL_DATA_WIDTH,
    parameter AXIL_APP_CTRL_ADDR_WIDTH = 24,
    parameter AXIL_APP_CTRL_STRB_WIDTH = (AXIL_APP_CTRL_DATA_WIDTH/8),

    parameter BUFFER_RX_LENGTH      = 32,
    parameter BUFFER_TX_LENGTH      = 32,
    parameter BUFFER_ELEM_MAX_SIZE  = 2*1024,
    parameter MAX_UDP_PORTS         = 1024
)
(
    // Clock: 25 MHz LVCMOS18
    input  wire       clk_25mhz_ref,

    // GPIO
    output wire [1:0] led,

    /*
     * Ethernet: 1000BASE-T RGMII
     */
    input  wire       phy_rx_clk,
    input  wire [3:0] phy_rxd,
    input  wire       phy_rx_ctl,
    output wire       phy_tx_clk,
    output wire [3:0] phy_txd,
    output wire       phy_tx_ctl,
    output wire       phy_reset_n,
    inout  wire       phy_mdio,
    output wire       phy_mdc,

    (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 clk_125mhz_o CLK" *)
    (* X_INTERFACE_PARAMETER = "FREQ_HZ 125000000, FREQ_TOLERANCE_HZ 0" *)
    output wire clk_125mhz_o,
    output wire rstn_125mhz_o ,

    // M_AXI for UDP packets (PL<->PS)
    (* X_INTERFACE_PARAMETER = "ASSOCIATED_BUSIF fpga_core_axi, ASSOCIATED_RESET fpga_core_axi_aresetn, FREQ_HZ 125000000, FREQ_TOLERANCE_HZ 0" *)
    input  wire         fpga_core_axi_aclk     ,  // just for Vivado integrator to infer clock/reset for axi interface
    input  wire         fpga_core_axi_aresetn  ,  // just for Vivado integrator to infer clock/reset for axi interface
    output wire [00:00] fpga_core_axi_arid     ,
    output wire [31:00] fpga_core_axi_araddr   ,
    output wire [07:00] fpga_core_axi_arlen    ,
    output wire [02:00] fpga_core_axi_arsize   ,
    output wire [01:00] fpga_core_axi_arburst  ,
    output wire         fpga_core_axi_arlock   ,
    output wire [03:00] fpga_core_axi_arcache  ,
    output wire [02:00] fpga_core_axi_arprot   ,
    output wire         fpga_core_axi_arvalid  ,
    input  wire         fpga_core_axi_arready  ,
    input  wire [01:00] fpga_core_axi_rid      ,
    input  wire [63:00] fpga_core_axi_rdata    ,
    input  wire [01:00] fpga_core_axi_rresp    ,
    input  wire         fpga_core_axi_rlast    ,
    input  wire         fpga_core_axi_rvalid   ,
    output wire         fpga_core_axi_rready   ,
    output wire [00:00] fpga_core_axi_awid     ,
    output wire [31:00] fpga_core_axi_awaddr   ,
    output wire [07:00] fpga_core_axi_awlen    ,
    output wire [02:00] fpga_core_axi_awsize   ,
    output wire [01:00] fpga_core_axi_awburst  ,
    output wire         fpga_core_axi_awlock   ,
    output wire [03:00] fpga_core_axi_awcache  ,
    output wire [02:00] fpga_core_axi_awprot   ,
    output wire         fpga_core_axi_awvalid  ,
    input  wire         fpga_core_axi_awready  ,
    output wire [63:00] fpga_core_axi_wdata    ,
    output wire [07:00] fpga_core_axi_wstrb    ,
    output wire         fpga_core_axi_wlast    ,
    output wire         fpga_core_axi_wvalid   ,
    input  wire         fpga_core_axi_wready   ,
    input  wire [07:00] fpga_core_axi_bid      ,
    input  wire [01:00] fpga_core_axi_bresp    ,
    input  wire         fpga_core_axi_bvalid   ,
    output wire         fpga_core_axi_bready   ,

    // Slave AXI lite (for synchonization with PS)
    (* X_INTERFACE_PARAMETER = "ASSOCIATED_BUSIF fpga_core_axil, ASSOCIATED_RESET fpga_core_axil_aresetn, FREQ_HZ 125000000, FREQ_TOLERANCE_HZ 0" *)
    input  wire         fpga_core_axil_aclk    , // just for Vivado integrator to infer clock/reset for axi interface
    input  wire         fpga_core_axil_aresetn , // just for Vivado integrator to infer clock/reset for axi interface
    input  wire [31:00] fpga_core_axil_awaddr  ,
    input  wire         fpga_core_axil_awvalid ,
    output wire         fpga_core_axil_awready ,
    input  wire [31:00] fpga_core_axil_wdata   ,
    input  wire [03:00] fpga_core_axil_wstrb   ,
    input  wire         fpga_core_axil_wvalid  ,
    output wire         fpga_core_axil_wready  ,
    output wire [01:00] fpga_core_axil_bresp   ,
    output wire         fpga_core_axil_bvalid  ,
    input  wire         fpga_core_axil_bready  ,
    input  wire [31:00] fpga_core_axil_araddr  ,
    input  wire         fpga_core_axil_arvalid ,
    output wire         fpga_core_axil_arready ,
    output wire [31:00] fpga_core_axil_rdata   ,
    output wire [01:00] fpga_core_axil_rresp   ,
    output wire         fpga_core_axil_rvalid  ,
    input  wire         fpga_core_axil_rready  ,

    // Interrupts
    output wire buffer_rx_pushed_interr

);

// Clock and reset
wire clk_25mhz_bufg;

// Internal 125 MHz clock
wire clk_125mhz_mmcm_out;
wire clk_125mhz_int;
wire rst_125mhz_int;

assign clk_125mhz_o = clk_125mhz_int; 
assign rstn_125mhz_o = ~rst_125mhz_int;

// wire mmcm_rst = reset;
wire mmcm_locked;
wire mmcm_clkfb;


// BUFG stands for "buffer gate." The BUFG primitive is used to create a 
// buffer gate, which is a digital circuit component that is used to 
// amplify and/or isolate a signal.
// 
// Using a BUFG gate helps to ensure that the clock signal is distributed 
// properly throughout the system and reaches all the necessary components 
// with minimal delay.
// 
// https://docs.xilinx.com/r/2022.1-English/ug974-vivado-ultrascale-libraries/BUFG
BUFG
clk_25mhz_bufg_in_inst (
    .I(clk_25mhz_ref),
    .O(clk_25mhz_bufg)
);

wire clk90_mmcm_out;
wire clk90_int;

wire clk_300_mmcm_out;
wire clk_300_int;
wire rst_300_int;

// Base Mixed Mode Clock Manager (MMCM)
// 
// used to implement a Phase-Locked Loop (PLL) with Multiplier/Multiplier 
// and Phase Shift (MMCM) functionality
// see https://docs.xilinx.com/r/2022.1-English/ug974-vivado-ultrascale-libraries/MMCME4_BASE

// MMCM instance
// 25 MHz in, 125 MHz out
// PFD range: 10 MHz to 500 MHz
// VCO range: 800 MHz to 1600 MHz
// M = 45, D = 1 sets Fvco = 1125 MHz (in range)
// Divide by 8 to get output frequency of 125 MHz
// Need two 125 MHz outputs with 90 degree offset
// Also need 300 MHz out for IODELAY
// 1125 / 3.75 = 300 MHz
MMCME4_BASE #(
    .BANDWIDTH("OPTIMIZED"),
    .CLKOUT0_DIVIDE_F(3.75),           // 1125 Mhz / 3.75 = 300 Mhz
    .CLKOUT0_DUTY_CYCLE(0.5),
    .CLKOUT0_PHASE(0),
    .CLKOUT1_DIVIDE(9),             // 1125 Mhz / 9 = 125 Mhz
    .CLKOUT1_DUTY_CYCLE(0.5),
    .CLKOUT1_PHASE(90),             // 90-degree phase shift
    .CLKOUT2_DIVIDE(9),             // 1125 Mhz / 9 = 125 Mhz
    .CLKOUT2_DUTY_CYCLE(0.5),
    .CLKOUT2_PHASE(0),
    .CLKOUT3_DIVIDE(1),
    .CLKOUT3_DUTY_CYCLE(0.5),
    .CLKOUT3_PHASE(0),
    .CLKOUT4_DIVIDE(1),
    .CLKOUT4_DUTY_CYCLE(0.5),
    .CLKOUT4_PHASE(0),
    .CLKOUT5_DIVIDE(1),
    .CLKOUT5_DUTY_CYCLE(0.5),
    .CLKOUT5_PHASE(0),
    .CLKOUT6_DIVIDE(1),
    .CLKOUT6_DUTY_CYCLE(0.5),
    .CLKOUT6_PHASE(0),
    .CLKFBOUT_MULT_F(45),           // 25 Mhz * 45 = 1125 Mhz VCO
    .CLKFBOUT_PHASE(0),
    .DIVCLK_DIVIDE(1),
    .REF_JITTER1(0.010),
    .CLKIN1_PERIOD(40.0),           // 25 Mhz clock in
    .STARTUP_WAIT("FALSE"),
    .CLKOUT4_CASCADE("FALSE")
)
clk_mmcm_inst (
    .CLKIN1(clk_25mhz_bufg),
    .CLKFBIN(mmcm_clkfb),
    .RST(1'b0),
    .PWRDWN(1'b0),
    .CLKOUT0(clk_300_mmcm_out),
    .CLKOUT0B(),
    .CLKOUT1(clk90_mmcm_out),
    .CLKOUT1B(),
    .CLKOUT2(clk_125mhz_mmcm_out),
    .CLKOUT2B(),
    .CLKOUT3(),
    .CLKOUT3B(),
    .CLKOUT4(),
    .CLKOUT5(),
    .CLKOUT6(),
    .CLKFBOUT(mmcm_clkfb),
    .CLKFBOUTB(),
    .LOCKED(mmcm_locked)
);

BUFG
clk_125mhz_bufg_inst (
    .I(clk_125mhz_mmcm_out),
    .O(clk_125mhz_int)
);

BUFG
clk90_bufg_inst (
    .I(clk90_mmcm_out),
    .O(clk90_int)
);

BUFG
clk_300_bufg_inst (
    .I(clk_300_mmcm_out),
    .O(clk_300_int)
);

sync_reset #(
    .N(4)
)
sync_reset_125mhz_inst (
    .clk(clk_125mhz_int),
    .rst(~mmcm_locked),
    .out(rst_125mhz_int)
);

sync_reset #(
    .N(4)
)
sync_reset_300mhz_inst (
    .clk(clk_300_int),
    .rst(~mmcm_locked),
    .out(rst_300_int)
);

// IODELAY elements for RGMII interface to PHY
wire [3:0] phy_rxd_delay;
wire       phy_rx_ctl_delay;

IDELAYCTRL #(
    .SIM_DEVICE("ULTRASCALE")
)
idelayctrl_inst
(
    .REFCLK(clk_300_int),
    .RST(rst_300_int),
    .RDY()
);

IDELAYE3 #(
    .CASCADE("NONE"),
    .REFCLK_FREQUENCY(300.0),
    .DELAY_FORMAT("TIME"),
    .IS_RST_INVERTED(1'b0),
    .DELAY_TYPE("FIXED"),
    .SIM_DEVICE("ULTRASCALE_PLUS"),
    .DELAY_VALUE(0)
)
phy_rxd_idelay_0
(
    .IDATAIN(phy_rxd[0]),
    .DATAOUT(phy_rxd_delay[0]),
    .CE(1'b0),
    .DATAIN(1'b0),
    .EN_VTC(1'b1),
    .INC(1'b0),
    .LOAD(1'b0),
    .RST(1'b0)
);

IDELAYE3 #(
    .CASCADE("NONE"),
    .REFCLK_FREQUENCY(300.0),
    .DELAY_FORMAT("TIME"),
    .IS_RST_INVERTED(1'b0),
    .DELAY_TYPE("FIXED"),
    .SIM_DEVICE("ULTRASCALE_PLUS"),
    .DELAY_VALUE(0)
)
phy_rxd_idelay_1
(
    .IDATAIN(phy_rxd[1]),
    .DATAOUT(phy_rxd_delay[1]),
    .CE(1'b0),
    .DATAIN(1'b0),
    .EN_VTC(1'b1),
    .INC(1'b0),
    .LOAD(1'b0),
    .RST(1'b0)
);

IDELAYE3 #(
    .CASCADE("NONE"),
    .REFCLK_FREQUENCY(300.0),
    .DELAY_FORMAT("TIME"),
    .IS_RST_INVERTED(1'b0),
    .DELAY_TYPE("FIXED"),
    .SIM_DEVICE("ULTRASCALE_PLUS"),
    .DELAY_VALUE(0)
)
phy_rxd_idelay_2
(
    .IDATAIN(phy_rxd[2]),
    .DATAOUT(phy_rxd_delay[2]),
    .CE(1'b0),
    .DATAIN(1'b0),
    .EN_VTC(1'b1),
    .INC(1'b0),
    .LOAD(1'b0),
    .RST(1'b0)
);

IDELAYE3 #(
    .CASCADE("NONE"),
    .REFCLK_FREQUENCY(300.0),
    .DELAY_FORMAT("TIME"),
    .IS_RST_INVERTED(1'b0),
    .DELAY_TYPE("FIXED"),
    .SIM_DEVICE("ULTRASCALE_PLUS"),
    .DELAY_VALUE(0)
)
phy_rxd_idelay_3
(
    .IDATAIN(phy_rxd[3]),
    .DATAOUT(phy_rxd_delay[3]),
    .CE(1'b0),
    .DATAIN(1'b0),
    .EN_VTC(1'b1),
    .INC(1'b0),
    .LOAD(1'b0),
    .RST(1'b0)
);

IDELAYE3 #(
    .CASCADE("NONE"),
    .REFCLK_FREQUENCY(300.0),
    .DELAY_FORMAT("TIME"),
    .IS_RST_INVERTED(1'b0),
    .DELAY_TYPE("FIXED"),
    .SIM_DEVICE("ULTRASCALE_PLUS"),
    .DELAY_VALUE(0)
)
phy_rx_ctl_idelay
(
    .IDATAIN(phy_rx_ctl),
    .DATAOUT(phy_rx_ctl_delay),
    .CE(1'b0),
    .DATAIN(1'b0),
    .EN_VTC(1'b1),
    .INC(1'b0),
    .LOAD(1'b0),
    .RST(1'b0)
);

reg [19:0] delay_reg = 20'hfffff;

reg [4:0] mdio_cmd_phy_addr = 5'h03;
reg [4:0] mdio_cmd_reg_addr = 5'h00;
reg [15:0] mdio_cmd_data = 16'd0;
reg [1:0] mdio_cmd_opcode = 2'b01;
reg mdio_cmd_valid = 1'b0;
wire mdio_cmd_ready;

reg [3:0] state_reg = 0;

always @(posedge clk_125mhz_int) begin
    if (rst_125mhz_int) begin
        state_reg <= 0;
        delay_reg <= 20'hfffff;
        mdio_cmd_reg_addr <= 5'h00;
        mdio_cmd_data <= 16'd0;
        mdio_cmd_valid <= 1'b0;
    end else begin
        mdio_cmd_valid <= mdio_cmd_valid & !mdio_cmd_ready;
        if (delay_reg > 0) begin
            delay_reg <= delay_reg - 1;
        end else if (!mdio_cmd_ready) begin
            // wait for ready
            state_reg <= state_reg;
        end else begin
            mdio_cmd_valid <= 1'b0;
            case (state_reg)
                // soft reset the PHY
                // write 0x8000 to CTRL (0x001F)
                4'd0: begin
                    // write to REGCR to load address
                    mdio_cmd_reg_addr <= 5'h0D;
                    mdio_cmd_data <= 16'h001F;
                    mdio_cmd_valid <= 1'b1;
                    state_reg <= 4'd1;
                end
                4'd1: begin
                    // write address of CTRL to ADDAR
                    mdio_cmd_reg_addr <= 5'h0E;
                    mdio_cmd_data <= 16'h001F;
                    mdio_cmd_valid <= 1'b1;
                    state_reg <= 4'd2;
                end
                4'd2: begin
                    // write to REGCR to load data
                    mdio_cmd_reg_addr <= 5'h0D;
                    mdio_cmd_data <= 16'h401F;
                    mdio_cmd_valid <= 1'b1;
                    state_reg <= 4'd3;
                end
                4'd3: begin
                    // write data for CTRL to ADDAR
                    mdio_cmd_reg_addr <= 5'h0E;
                    mdio_cmd_data <= 16'h8000;
                    mdio_cmd_valid <= 1'b1;
                    state_reg <= 4'd4;
                end
                4'd4: begin
                    // done
                    state_reg <= 4'd4;
                end
            endcase
        end
    end
end

wire mdc;
wire mdio_i;
wire mdio_o;
wire mdio_t;

mdio_master
mdio_master_inst (
    .clk(clk_125mhz_int),
    .rst(rst_125mhz_int),

    .cmd_phy_addr(mdio_cmd_phy_addr),
    .cmd_reg_addr(mdio_cmd_reg_addr),
    .cmd_data(mdio_cmd_data),
    .cmd_opcode(mdio_cmd_opcode),
    .cmd_valid(mdio_cmd_valid),
    .cmd_ready(mdio_cmd_ready),

    .data_out(),
    .data_out_valid(),
    .data_out_ready(1'b1),

    .mdc_o(mdc),
    .mdio_i(mdio_i),
    .mdio_o(mdio_o),
    .mdio_t(mdio_t),

    .busy(),

    .prescale(8'd3)
);

assign phy_mdc = mdc;
assign mdio_i = phy_mdio;
assign phy_mdio = mdio_t ? 1'bz : mdio_o;

fpga_core #(
    .TARGET("XILINX"),
    .BUFFER_RX_LENGTH     (BUFFER_RX_LENGTH    ),
    .BUFFER_TX_LENGTH     (BUFFER_TX_LENGTH    ),
    .BUFFER_ELEM_MAX_SIZE (BUFFER_ELEM_MAX_SIZE),
    .MAX_UDP_PORTS        (MAX_UDP_PORTS       )
) core_inst (
    /*
     * Clock: 125MHz
     * Synchronous reset
     */
    .clk(clk_125mhz_int),
    .clk90(clk90_int),
    .rst(rst_125mhz_int),
    /*
     * GPIO
     */
    .led(led),
    /*
     * Ethernet: 1000BASE-T RGMII
     */
    .phy_rx_clk(phy_rx_clk),
    .phy_rxd(phy_rxd_delay),
    .phy_rx_ctl(phy_rx_ctl_delay),
    .phy_tx_clk(phy_tx_clk),
    .phy_txd(phy_txd),
    .phy_tx_ctl(phy_tx_ctl),
    .phy_reset_n(phy_reset_n),
    /*
     * Master AXI (for udp rx/tx)
     */    
    .m_axi_arid    (fpga_core_axi_arid   ),
    .m_axi_araddr  (fpga_core_axi_araddr ),
    .m_axi_arlen   (fpga_core_axi_arlen  ),
    .m_axi_arsize  (fpga_core_axi_arsize ),
    .m_axi_arburst (fpga_core_axi_arburst),
    .m_axi_arlock  (fpga_core_axi_arlock ),
    .m_axi_arcache (fpga_core_axi_arcache),
    .m_axi_arprot  (fpga_core_axi_arprot ),
    .m_axi_arvalid (fpga_core_axi_arvalid),
    .m_axi_arready (fpga_core_axi_arready),
    .m_axi_rid     (fpga_core_axi_rid    ),
    .m_axi_rdata   (fpga_core_axi_rdata  ),
    .m_axi_rresp   (fpga_core_axi_rresp  ),
    .m_axi_rlast   (fpga_core_axi_rlast  ),
    .m_axi_rvalid  (fpga_core_axi_rvalid ),
    .m_axi_rready  (fpga_core_axi_rready ),
    .m_axi_awid    (fpga_core_axi_awid   ),
    .m_axi_awaddr  (fpga_core_axi_awaddr ),
    .m_axi_awlen   (fpga_core_axi_awlen  ),
    .m_axi_awsize  (fpga_core_axi_awsize ),
    .m_axi_awburst (fpga_core_axi_awburst),
    .m_axi_awlock  (fpga_core_axi_awlock ),
    .m_axi_awcache (fpga_core_axi_awcache),
    .m_axi_awprot  (fpga_core_axi_awprot ),
    .m_axi_awvalid (fpga_core_axi_awvalid),
    .m_axi_awready (fpga_core_axi_awready),
    .m_axi_wdata   (fpga_core_axi_wdata  ),
    .m_axi_wstrb   (fpga_core_axi_wstrb  ),
    .m_axi_wlast   (fpga_core_axi_wlast  ),
    .m_axi_wvalid  (fpga_core_axi_wvalid ),
    .m_axi_wready  (fpga_core_axi_wready ),
    .m_axi_bid     (fpga_core_axi_bid    ),
    .m_axi_bresp   (fpga_core_axi_bresp  ),
    .m_axi_bvalid  (fpga_core_axi_bvalid ),
    .m_axi_bready  (fpga_core_axi_bready ),
    /*
     * Slave AXI lite (for synchonization with PS)
     */    
    .s_axil_awaddr (fpga_core_axil_awaddr ),
    .s_axil_awvalid(fpga_core_axil_awvalid),
    .s_axil_awready(fpga_core_axil_awready),
    .s_axil_wdata  (fpga_core_axil_wdata  ),
    .s_axil_wstrb  (fpga_core_axil_wstrb  ),
    .s_axil_wvalid (fpga_core_axil_wvalid ),
    .s_axil_wready (fpga_core_axil_wready ),
    .s_axil_bresp  (fpga_core_axil_bresp  ),
    .s_axil_bvalid (fpga_core_axil_bvalid ),
    .s_axil_bready (fpga_core_axil_bready ),
    .s_axil_araddr (fpga_core_axil_araddr ),
    .s_axil_arvalid(fpga_core_axil_arvalid),
    .s_axil_arready(fpga_core_axil_arready),
    .s_axil_rdata  (fpga_core_axil_rdata  ),
    .s_axil_rresp  (fpga_core_axil_rresp  ),
    .s_axil_rvalid (fpga_core_axil_rvalid ),
    .s_axil_rready (fpga_core_axil_rready ),
    /*
     * Interrupts
     */    
    .buffer_rx_pushed_interr_o (buffer_rx_pushed_interr)
);

endmodule

`resetall
