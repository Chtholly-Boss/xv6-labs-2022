# Networking
[MIT6.1810 Networking Page](https://pdos.csail.mit.edu/6.S081/2022/labs/net.html)

This lab teaches you how to write a driver for a specific hardware, however, it's not so interesting because the **Friendly** Manual is too long and full of things we don't need to know.

So I finish this lab just by following the hints and write code that __I think it can run__. If it couldn't run? ...maybe I will refer to someone else for help. Orz

## transmit
```c
int
e1000_transmit(struct mbuf *m)
{
  //
  // Your code here.
  //
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  //
  acquire(&e1000_lock);
  uint32 idx = regs[E1000_TDT];
  struct tx_desc *desc = &tx_ring[idx];
  if ((desc->status & E1000_TXD_STAT_DD) == 0){
    printf("e1000 transmit: error.\n");
    release(&e1000_lock);
    return -1;
  }
  if (tx_mbufs[idx]){
    mbuffree(tx_mbufs[idx]);
    tx_mbufs[idx] = 0;
  }
  desc->addr = (uint64)m->head;
  desc->length = (uint16)m->len;
  desc->cmd |= E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  tx_mbufs[idx] = m;
  // Update the TDT
  regs[E1000_TDT] = (regs[E1000_TDT] + 1) % TX_RING_SIZE;
  release(&e1000_lock);
  return 0;
}
```
## recv
```c
static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver an mbuf for each packet (using net_rx()).
  //
  // ! Don't add lock here. lock has been acquired and released in net
  uint32 idx = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
  struct rx_desc *desc = &rx_ring[idx];
  while(desc->status & E1000_RXD_STAT_DD){
    rx_mbufs[idx]->len = desc->length;
    net_rx(rx_mbufs[idx]);
    rx_mbufs[idx] = mbufalloc(0);
    if(!rx_mbufs[idx]){
      panic("e1000");
    }
    desc->addr = (uint64)rx_mbufs[idx]->head;
    desc->status = 0;
    regs[E1000_RDT] = idx;
    idx = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
    desc = &rx_ring[idx];
  }
  return;
}
```
## Test
run `make grade` to test your code. Don't forget to add a `time.txt` to get a full score.