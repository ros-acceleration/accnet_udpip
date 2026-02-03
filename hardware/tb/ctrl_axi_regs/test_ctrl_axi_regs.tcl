# add_waves.tcl 
set sig_list {        \
    clk_i             \
    rst_i             \
}

gtkwave::addSignalsFromList $sig_list

# Zoom full (Shift + Alt + F)
gtkwave::/Time/Zoom/Zoom_Full

# Change signal formats
# gtkwave::/Edit/Highlight_Regexp "head_index_o"
gtkwave::/Edit/Data_Format/Decimal
gtkwave::/Edit/UnHighlight_All

