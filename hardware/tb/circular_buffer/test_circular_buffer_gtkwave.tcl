# add_waves.tcl 
set sig_list {        \
    clk_i             \
    rst_i             \
    data_pushed_i     \
    data_popped_i     \
    head_index_o      \
    tail_index_o      \
    empty_o           \
    full_o            \
}

gtkwave::addSignalsFromList $sig_list

# Zoom full (Shift + Alt + F)
gtkwave::/Time/Zoom/Zoom_Full

# Change signal formats
gtkwave::/Edit/Highlight_Regexp "head_index_o"
gtkwave::/Edit/Highlight_Regexp "tail_index_o"
gtkwave::/Edit/Data_Format/Decimal
gtkwave::/Edit/UnHighlight_All

