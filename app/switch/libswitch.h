#ifndef LIBSWITCH_H
#define LIBSWITCH_H

int switch_open();
void switch_close(int sock);

int switch_phy_read(int sock, int phy_addr, int reg, int *val);
int switch_phy_write(int sock, int phy_addr, int reg, int val);
int switch_phy_status(int sock, int phy_addr, int *link, int *speed, int *duplex);

int switch_sw_read(int sock, int reg, int *val);
int switch_sw_write(int sock, int reg, int val);

#endif

