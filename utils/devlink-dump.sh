#!/bin/bash
# ------------------------------------------------------------------------------
# Dump the device registers' value using devlink
# ------------------------------------------------------------------------------

# Device and region details
device="platform/a0010000.fpga/registers"

# Function to check if a snapshot exists
check_snapshot() {
    # Get the output of the devlink region show command
    snapshot_list=$(sudo devlink region show $device 2>/dev/null)
    
    # Extract the part after 'snapshot [' and remove ']' to get the snapshot number (if any)
    snapshot_number=$(echo "$snapshot_list" | grep -oP 'snapshot \[\K[0-9]*')

    if [[ -n "$snapshot_number" ]]; then
        echo "Snapshot $snapshot_number exists for device $device."
        return 0  # Snapshot exists
    else
        echo "No snapshot exists for device $device."
        return 1  # No snapshot exists
    fi
}

# Function to delete a snapshot
delete_snapshot() {
    echo "Deleting snapshot $snapshot_number..."
    sudo devlink region del $device snapshot $snapshot_number
    echo "Snapshot $snapshot_number deleted."
}

# Function to create a new snapshot
create_new_snapshot() {
    # Get the current max snapshot number and increment it
    if [[ -n "$snapshot_number" ]]; then
        new_snapshot=$((snapshot_number + 1))
    else
        new_snapshot=0
    fi
    
    echo "Creating new snapshot $new_snapshot..."
    sudo devlink region new $device snapshot $new_snapshot
    echo "New snapshot $new_snapshot created."
}

# ------------------------------------------------------------------------------
# Dump the device registers' value using devlink
# ------------------------------------------------------------------------------

# Check if a snapshot exists
if check_snapshot; then
    # Snapshot exists, ask user for input
    echo "Do you want to (P)roceed with this snapshot or (D)elete and create a new one? (P/D)"
    read -r user_choice
    if [[ "$user_choice" == "D" || "$user_choice" == "d" ]]; then
        # Delete the existing snapshot and create a new one
        delete_snapshot
        create_new_snapshot
        snapshot_number=$new_snapshot
    fi
else
    # No snapshot exists, create a new one
    create_new_snapshot
    snapshot_number=$new_snapshot
fi


# Define the register names and their corresponding addresses
declare -A registers=(
  ["RBTC_CTRL_ADDR_AP_CTRL_0_N_P"]="0x00000000"
  ["RBTC_CTRL_ADDR_RES_0_Y_O"]="0x00000008"
  ["RBTC_CTRL_ADDR_MAC_0_N_O"]="0x00000010"
  ["RBTC_CTRL_ADDR_MAC_1_N_O"]="0x00000018"
  ["RBTC_CTRL_ADDR_GW_0_N_O"]="0x00000020"
  ["RBTC_CTRL_ADDR_SNM_0_N_O"]="0x00000028"
  ["RBTC_CTRL_ADDR_IP_LOC_0_N_O"]="0x00000030"
  ["RBTC_CTRL_ADDR_UDP_RANGE_L_0_N_O"]="0x00000038"
  ["RBTC_CTRL_ADDR_UDP_RANGE_H_0_N_O"]="0x00000040"
  ["RBTC_CTRL_ADDR_SHMEM_0_N_O"]="0x00000048"
  ["RBTC_CTRL_ADDR_ISR0"]="0x00000050"
  ["RBTC_CTRL_ADDR_IER0"]="0x00000058"
  ["RBTC_CTRL_ADDR_GIE"]="0x00000060"
  ["RBTC_CTRL_ADDR_BUFTX_HEAD_0_N_I"]="0x00000068"
  ["RBTC_CTRL_ADDR_BUFTX_TAIL_0_N_I"]="0x00000070"
  ["RBTC_CTRL_ADDR_BUFTX_EMPTY_0_N_I"]="0x00000078"
  ["RBTC_CTRL_ADDR_BUFTX_FULL_0_N_I"]="0x00000080"
  ["RBTC_CTRL_ADDR_BUFTX_PUSHED_0_Y_O"]="0x00000088"
  ["RBTC_CTRL_ADDR_BUFTX_POPPED_0_N_I"]="0x00000090"
  ["RBTC_CTRL_ADDR_BUFRX_PUSH_IRQ_0_IRQ"]="0x00000098"
  ["RBTC_CTRL_ADDR_BUFRX_OFFSET_0_N_I"]="0x000000a0"
)

# Define the order of the registers (explicit list)
register_order=(
  "RBTC_CTRL_ADDR_AP_CTRL_0_N_P"
  "RBTC_CTRL_ADDR_RES_0_Y_O"
  "RBTC_CTRL_ADDR_MAC_0_N_O"
  "RBTC_CTRL_ADDR_MAC_1_N_O"
  "RBTC_CTRL_ADDR_GW_0_N_O"
  "RBTC_CTRL_ADDR_SNM_0_N_O"
  "RBTC_CTRL_ADDR_IP_LOC_0_N_O"
  "RBTC_CTRL_ADDR_UDP_RANGE_L_0_N_O"
  "RBTC_CTRL_ADDR_UDP_RANGE_H_0_N_O"
  "RBTC_CTRL_ADDR_SHMEM_0_N_O"
  "RBTC_CTRL_ADDR_ISR0"
  "RBTC_CTRL_ADDR_IER0"
  "RBTC_CTRL_ADDR_GIE"
  "RBTC_CTRL_ADDR_BUFTX_HEAD_0_N_I"
  "RBTC_CTRL_ADDR_BUFTX_TAIL_0_N_I"
  "RBTC_CTRL_ADDR_BUFTX_EMPTY_0_N_I"
  "RBTC_CTRL_ADDR_BUFTX_FULL_0_N_I"
  "RBTC_CTRL_ADDR_BUFTX_PUSHED_0_Y_O"
  "RBTC_CTRL_ADDR_BUFTX_POPPED_0_N_I"
  "RBTC_CTRL_ADDR_BUFRX_PUSH_IRQ_0_IRQ"
  "RBTC_CTRL_ADDR_BUFRX_OFFSET_0_N_I"
)

# Create a new snapshot
#sudo devlink region del $device snapshot $snapshot
#sudo devlink region new $device

# Print a header for the output table
# printf "%-35s | %-15s | %-15s\n" "Register Name" "Address" "Value"
# echo "------------------------------------|-----------------|----------------"

printf "%-35s | %-15s | %-15s | %-15s\n" "Register Name" "Address" "Value" "Custom Value"
echo "------------------------------------|-----------------|-----------------|-------------------------"

# Iterate over each register and read its value
for reg_name in "${register_order[@]}"; do
    reg_addr_hex=${registers[$reg_name]}
    
    # Convert hex address to decimal
    reg_addr_dec=$((reg_addr_hex))

    # Execute the devlink command to read the value at the given decimal address
    output=$(sudo devlink region read "$device" snapshot "$snapshot_number" address "$reg_addr_dec" length 8 2>/dev/null)
 
    # Extract the value bytes (ignoring the first part which is the address)
    value_bytes=$(echo "$output" | awk '{for (i=2; i<=NF; i++) printf $i}')

    # Reverse the value bytes when needed for endianess
    reversed_value=$(echo "$value_bytes" | sed 's/../& /g' | awk '{for (i=NF-4; i>0; i--) printf $i}')

    # Convert the hexadecimal bytes to a single integer value
    value_int=$((16#$value_bytes))

    # Initialize custom value
    custom_value=""

    # Process custom values based on register name
    case "$reg_name" in
        "RBTC_CTRL_ADDR_MAC_0_N_O" | "RBTC_CTRL_ADDR_MAC_1_N_O")
            # Take lower 32 bits and format as MAC address
            mac_address=$(printf "%02x:%02x:%02x" \
                $(( (value_int & 0xFFFFFFFF00000000) >> 56 )) \
                $(( (value_int & 0x00FFFFFFFF000000) >> 48 )) \
                $(( (value_int & 0x0000FFFFFFFF0000) >> 40 )))
            custom_value=$mac_address
            ;;
        "RBTC_CTRL_ADDR_GW_0_N_O" | "RBTC_CTRL_ADDR_IP_LOC_0_N_O" | "RBTC_CTRL_ADDR_SNM_0_N_O")
            # Convert to IPv4 address
            ip_address=$(printf "%d.%d.%d.%d" \
                $(( (value_int >> 32) & 0xFF )) \
                $(( (value_int >> 40) & 0xFF )) \
                $(( (value_int >> 48) & 0xFF )) \
                $(( (value_int >> 56) & 0xFF )) )
            custom_value=$ip_address
            ;;
        "RBTC_CTRL_ADDR_SHMEM_0_N_O")
            # Convert to 32 bit address
            custom_value="0x"$reversed_value
            ;;
        "RBTC_CTRL_ADDR_BUFTX_HEAD_0_N_I" | "RBTC_CTRL_ADDR_BUFTX_TAIL_0_N_I")
            custom_value=$((16#$reversed_value))
            ;;
        "RBTC_CTRL_ADDR_BUFRX_OFFSET_0_N_I")
            # Decode bit fields for RBTC_CTRL_BUFRX struct
            value_int_reversed=$((16#$reversed_value))
            popped=$(( (value_int_reversed >> 0) & 0x1 ))
            pushed=$(( (value_int_reversed >> 1) & 0x1 ))
            full=$(( (value_int_reversed >> 2) & 0x1 ))
            empty=$(( (value_int_reversed >> 3) & 0x1 ))
            tail=$(( (value_int_reversed >> 4) & 0x1F ))  # 5 bits
            head=$(( (value_int_reversed >> 9) & 0x1F ))  # 5 bits
            socket_state=$(( (value_int_reversed >> 14) & 0x1 ))
            dummy=$(( (value_int_reversed >> 15) & 0x1 ))
            custom_value="popd=$popped, pushd=$pushed, fll=$full, empty=$empty, tail=$tail, head=$head, sock_state=$socket_state, dummy=$dummy"
            ;;
        *)
            custom_value=""  # No custom value for other registers
            ;;
    esac

    # Print the register name, address (decimal), and value (integer)
    printf "%-35s | %-15s | %-15s | %-25s\n" "$reg_name" "$reg_addr_hex" "0x$reversed_value" "$custom_value"
done

