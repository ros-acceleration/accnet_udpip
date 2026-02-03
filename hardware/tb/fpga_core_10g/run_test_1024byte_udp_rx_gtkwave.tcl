# Remove all signals currently traced
gtkwave::/Edit/Highlight_Regexp "."
gtkwave::/Edit/Delete
gtkwave::/Edit/UnHighlight_All

# add_waves.tcl 
set sig_list {                                 \
    clk                                        \
    rst                                        \
    sfp0_rxd                                   \
    sfp0_rxc                                   \
                                               \
    controller_inst.rst_user_req               \
    controller_inst.rst_user                   \
    controller_inst.rst_global                 \
    controller_inst.rx_busy                    \
                                               \
    ctrl_axi_regs_inst.s_axil_awaddr           \
    ctrl_axi_regs_inst.s_axil_awvalid          \
    ctrl_axi_regs_inst.s_axil_awready          \
    ctrl_axi_regs_inst.s_axil_wdata            \
    ctrl_axi_regs_inst.s_axil_wstrb            \
    ctrl_axi_regs_inst.s_axil_wvalid           \
    ctrl_axi_regs_inst.s_axil_wready           \
    ctrl_axi_regs_inst.s_axil_bresp            \
    ctrl_axi_regs_inst.s_axil_bvalid           \
    ctrl_axi_regs_inst.s_axil_bready           \
    ctrl_axi_regs_inst.s_axil_araddr           \
    ctrl_axi_regs_inst.s_axil_arvalid          \
    ctrl_axi_regs_inst.s_axil_arready          \
    ctrl_axi_regs_inst.s_axil_rdata            \
    ctrl_axi_regs_inst.s_axil_rresp            \
    ctrl_axi_regs_inst.s_axil_rvalid           \
    ctrl_axi_regs_inst.s_axil_rready           \
    ctrl_axi_regs_inst.ap_start                \
    ctrl_axi_regs_inst.ap_done                 \
    ctrl_axi_regs_inst.ap_ready                \
    ctrl_axi_regs_inst.ap_idle                 \
    ctrl_axi_regs_inst.interrupt               \
    ctrl_axi_regs_inst.user_rst_o              \
    ctrl_axi_regs_inst.local_mac_o             \
    ctrl_axi_regs_inst.gateway_ip_o            \
    ctrl_axi_regs_inst.subnet_mask_o           \
    ctrl_axi_regs_inst.local_ip_o              \
    ctrl_axi_regs_inst.ip_listen_l_o           \
    ctrl_axi_regs_inst.ip_listen_h_o           \
    ctrl_axi_regs_inst.shared_mem_o            \
    ctrl_axi_regs_inst.bufrx_head_i            \
    ctrl_axi_regs_inst.bufrx_tail_i            \
    ctrl_axi_regs_inst.bufrx_empty_i           \
    ctrl_axi_regs_inst.bufrx_full_i            \
    ctrl_axi_regs_inst.bufrx_pushed_i          \
    ctrl_axi_regs_inst.bufrx_popped_o          \
    ctrl_axi_regs_inst.bufrx_push_irq_i        \
    ctrl_axi_regs_inst.buftx_head_i            \
    ctrl_axi_regs_inst.buftx_tail_i            \
    ctrl_axi_regs_inst.buftx_empty_i           \
    ctrl_axi_regs_inst.buftx_full_i            \
    ctrl_axi_regs_inst.buftx_pushed_o          \
    ctrl_axi_regs_inst.buftx_popped_i          \
                                               \
    controller_inst.local_mac                  \
    controller_inst.gateway_ip                 \
    controller_inst.subnet_mask                \
    controller_inst.local_ip                   \
    controller_inst.rx_hdr_ready               \
    controller_inst.rx_hdr_valid               \
    controller_inst.rx_hdr_source_ip           \
    controller_inst.rx_hdr_source_port         \
    controller_inst.rx_hdr_dest_ip             \
    controller_inst.rx_hdr_dest_port           \
    controller_inst.rx_hdr_udp_length          \
    controller_inst.rx_payload_axis_tready     \
    controller_inst.rx_payload_axis_tvalid     \
    controller_inst.rx_payload_axis_tdata      \
    controller_inst.rx_payload_axis_tkeep      \
    controller_inst.rx_payload_axis_tlast      \
    controller_inst.rx_payload_axis_tuser      \
                                               \
    controller_inst.circbuff_rx_data_pushed    \
    controller_inst.circbuff_rx_data_popped    \
    controller_inst.circbuff_rx_head_index     \
    controller_inst.circbuff_rx_tail_index     \
    controller_inst.circbuff_rx_full           \
    controller_inst.circbuff_rx_empty          \
                                               \
    controller_inst.shared_mem_base_address    \
    controller_inst.dma_wr_ctrl_addr_o         \
    controller_inst.dma_wr_ctrl_addr_o         \
    controller_inst.dma_wr_ctrl_len_bytes_o    \
    controller_inst.dma_wr_ctrl_valid_o        \
    controller_inst.dma_wr_ctrl_ready_i        \
    controller_inst.dma_wr_ctrl_popped_i       \
    controller_inst.dma_wr_data_axis_tready    \
    controller_inst.dma_wr_data_axis_tvalid    \
    controller_inst.dma_wr_data_axis_tlast     \
    controller_inst.dma_wr_data_axis_tdata     \
    controller_inst.dma_wr_data_axis_tkeep     \
                                               \
    m_axi_awid                                 \ 
    m_axi_awaddr                               \ 
    m_axi_awlen                                \ 
    m_axi_awsize                               \ 
    m_axi_awburst                              \ 
    m_axi_awlock                               \ 
    m_axi_awcache                              \ 
    m_axi_awprot                               \ 
    m_axi_awvalid                              \ 
    m_axi_awready                              \ 
    m_axi_wdata                                \ 
    m_axi_wstrb                                \ 
    m_axi_wlast                                \ 
    m_axi_wvalid                               \ 
    m_axi_wready                               \ 
    m_axi_bid                                  \ 
    m_axi_bresp                                \ 
    m_axi_bvalid                               \ 
    m_axi_bready                               \
}

gtkwave::addSignalsFromList $sig_list

# Zoom full (Shift + Alt + F)
gtkwave::/Time/Zoom/Zoom_Full

# Change signal formats
# gtkwave::/Edit/Highlight_Regexp "axi_dma_wr_inst.clk"
# gtkwave::/Edit/Data_Format/Hexadecimal
# gtkwave::/Edit/UnHighlight_All

