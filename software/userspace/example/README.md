# Example programs

These folder contains a few example programs that can be used to carry out simple connectivity test.
Specifically, the folder contains the following programs:
- `rawsend.c` : Sends a UDP packet using a raw ethernet socket
- `rawrecv.c` : Receives a UDP packet using a raw ethernet socket
- `send.c` : Sends a UDP packet using a classic socket
- `recv.c` : Receives a UDP packet using a classic socket
- `echo-back-ps.c` : Receives a UDP packet using a classic socket on the PS GbE interface
- `echo-back-pl.c` : Receives a UDP packet using a classic socket on the PL GbE interface

Examples can be compiled using the provided Makefile:

```bash
make all
```

Network parameters, such as source and destination IPs and port numbers can be customized making changes to `config.h`.
