#include "blescan.h"
#include <sys/socket.h>
#include <bluetooth/l2cap.h>

int main(int argc, char *argv[]){
	int id_type;
	char *id;
	bdaddr_t src_addr;
	uint16_t le_conn_handle = 0;
	char dest_addr_str[18];

	if (argc < 3){
		perror("Insufficient arguments");
		exit(0);
	}
	id = argv[2];
	if (strcmp(argv[1], "-m") == 0){
		id_type = ID_TYPE_MAC;
	}
	else if (strcmp(argv[1], "-n") == 0){
		id_type = ID_TYPE_NAME;
	}
	else{
		perror("Unknow argument");
		exit(0);
	}
	memset(dest_addr_str, 0, sizeof(dest_addr_str));
	int res = blescan(id_type, id, 100, dest_addr_str);
	if (res == 0){  // 未发现相应的节点
		perror("Can not find the target device");
		exit(0);
	}
	printf("find the target\n");

	int device_id = hci_get_route(NULL);
	int hci_socket = hci_open_dev(device_id);
	hci_devba(device_id, &src_addr);
	// 创建BLE连接

	bdaddr_t dest_bdaddr;
	memset(&dest_bdaddr, 0, sizeof(dest_bdaddr));
	str2ba(dest_addr_str, &dest_bdaddr);
	res = hci_le_create_conn(hci_socket, htobs(0x04), htobs(0x04), 0, LE_RANDOM_ADDRESS,
															dest_bdaddr, LE_PUBLIC_ADDRESS, htobs(0x0018), htobs(0x0028), htobs(0), htobs(0x0100),
															htobs(0x0000), htobs(0x0010), &le_conn_handle, 25000);
	if (res != 0){
		perror("Connection error");
		goto hciclose;
	}
	struct sockaddr_l2 dest_sockaddr, src_sockaddr;
	memset(&dest_sockaddr, 0, sizeof(dest_sockaddr));
	memset(&src_sockaddr, 0, sizeof(src_sockaddr));

	dest_sockaddr.l2_family = AF_BLUETOOTH;
	dest_sockaddr.l2_psm = htobs(0x0235);
//	dest_sockaddr.l2_cid = htobs(0x0235);
//	dest_sockaddr.l2_bdaddr_type = BDADDR_LE_RANDOM;
	str2ba(dest_addr_str, &dest_sockaddr.l2_bdaddr);

	int l2cap_client_socket = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
	if (l2cap_client_socket < 0){
		perror("Can not open l2cap socket");
		goto hciclose;
	}
	res = connect(l2cap_client_socket, (struct sockaddr *)&dest_sockaddr, sizeof(dest_sockaddr));
	if (res == 0){
		res = write(l2cap_client_socket, "hello from linux", 17);
	}
	else if(res < 0){
		printf("error code = %d\n", res);
		perror("l2cap socket connction failed");
		hci_disconnect(hci_socket, le_conn_handle, HCI_OE_USER_ENDED_CONNECTION, 1000);
		goto hciclose;
	}

	while (1){
		char message[20];
		scanf("%s", message);
		res = write(l2cap_client_socket, message, strlen(message));
		if (res < 0){
			perror("message send failed");
			hci_disconnect(hci_socket, le_conn_handle, HCI_OE_USER_ENDED_CONNECTION, 1000);
			goto hciclose;
		}
	}

hciclose:
	if (le_conn_handle != 0){
		hci_disconnect(hci_socket, le_conn_handle, HCI_OE_USER_ENDED_CONNECTION, 1000);
	}
	hci_close_dev(device_id);
	exit(0);



}
