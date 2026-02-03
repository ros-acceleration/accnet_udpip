# Remove all signals currently traced
gtkwave::/Edit/Highlight_Regexp "."
gtkwave::/Edit/Delete
gtkwave::/Edit/UnHighlight_All

# add_waves.tcl 
set sig_list {                                 \
    clk                                        \
    rst                                        \
                                               \
    controller_inst.rst_user_req               \
    controller_inst.rst_user                   \
    controller_inst.rst_global                 \
    controller_inst.tx_busy                    \
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
    m_axi_arid                                 \
    m_axi_araddr                               \
    m_axi_arlen                                \
    m_axi_arsize                               \
    m_axi_arburst                              \
    m_axi_arlock                               \
    m_axi_arcache                              \
    m_axi_arprot                               \
    m_axi_arvalid                              \
    m_axi_arready                              \
    m_axi_rid                                  \
    m_axi_rdata                                \
    m_axi_rresp                                \
    m_axi_rlast                                \
    m_axi_rvalid                               \
    m_axi_rready                               \
                                               \
    controller_inst.shared_mem_base_address    \
    controller_inst.dma_rd_ctrl_addr_o         \
    controller_inst.dma_rd_ctrl_addr_o         \
    controller_inst.dma_rd_ctrl_len_bytes_o    \
    controller_inst.dma_rd_ctrl_valid_o        \
    controller_inst.dma_rd_ctrl_ready_i        \
    controller_inst.dma_rd_ctrl_popped_i       \
    controller_inst.dma_rd_data_axis_tready    \
    controller_inst.dma_rd_data_axis_tvalid    \
    controller_inst.dma_rd_data_axis_tlast     \
    controller_inst.dma_rd_data_axis_tdata     \
    controller_inst.dma_rd_data_axis_tkeep     \
                                               \
    axis_header_remover_inst.s_axis_tready     \
    axis_header_remover_inst.s_axis_tvalid     \
    axis_header_remover_inst.s_axis_tdata      \
    axis_header_remover_inst.s_axis_tkeep      \
    axis_header_remover_inst.s_axis_tlast      \
    axis_header_remover_inst.s_axis_tuser      \
    axis_header_remover_inst.state             \
    axis_header_remover_inst.count_header      \
    axis_header_remover_inst.s_axis_consumed   \
    axis_header_remover_inst.transf_count      \
    axis_header_remover_inst.transf_done       \
    axis_header_remover_inst.hdr_valid         \
    axis_header_remover_inst.hdr_source_ip     \
    axis_header_remover_inst.hdr_source_port   \
    axis_header_remover_inst.hdr_dest_ip       \
    axis_header_remover_inst.hdr_dest_port     \
    axis_header_remover_inst.hdr_udp_length    \
    axis_header_remover_inst.axis_forwarded_tready\
    axis_header_remover_inst.axis_forwarded_tvalid\
    axis_header_remover_inst.axis_forwarded_tdata \
    axis_header_remover_inst.axis_forwarded_tkeep \
    axis_header_remover_inst.axis_forwarded_tlast \
    axis_header_remover_inst.axis_forwarded_tuser \
    axis_header_remover_inst.m_axis_tready     \
    axis_header_remover_inst.m_axis_tvalid     \
    axis_header_remover_inst.m_axis_tdata      \
    axis_header_remover_inst.m_axis_tkeep      \
    axis_header_remover_inst.m_axis_tlast      \
    axis_header_remover_inst.m_axis_tuser      \
                                               \
    controller_inst.circbuff_tx_data_pushed    \
    controller_inst.circbuff_tx_data_popped    \
    controller_inst.circbuff_tx_head_index     \
    controller_inst.circbuff_tx_tail_index     \
    controller_inst.circbuff_tx_full           \
    controller_inst.circbuff_tx_empty          \
                                               \
    controller_inst.local_mac                  \
    controller_inst.gateway_ip                 \
    controller_inst.subnet_mask                \
    controller_inst.local_ip                   \
    controller_inst.tx_hdr_ready               \
    controller_inst.tx_hdr_valid               \
    controller_inst.tx_hdr_source_ip           \
    controller_inst.tx_hdr_source_port         \
    controller_inst.tx_hdr_dest_ip             \
    controller_inst.tx_hdr_dest_port           \
    controller_inst.tx_hdr_udp_length          \
    controller_inst.tx_payload_axis_tready     \
    controller_inst.tx_payload_axis_tvalid     \
    controller_inst.tx_payload_axis_tdata      \
    controller_inst.tx_payload_axis_tkeep      \
    controller_inst.tx_payload_axis_tlast      \
    controller_inst.tx_payload_axis_tuser      \
                                               \
    sfp0_txd                                   \
    sfp0_txc                                   \
}

gtkwave::addSignalsFromList $sig_list

# Zoom full (Shift + Alt + F)
gtkwave::/Time/Zoom/Zoom_Full

# Change signal formats
gtkwave::/Edit/Highlight_Regexp "transf_count"
gtkwave::/Edit/Highlight_Regexp "count_header"
gtkwave::/Edit/Data_Format/Decimal
gtkwave::/Edit/UnHighlight_All
