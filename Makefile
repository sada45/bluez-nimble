CC = gcc
PROM = l2cap_client

$(PROM):blescan.c l2cap_client.c
	$(CC) -o $(PROM) blescan.c l2cap_client.c -lbluetooth