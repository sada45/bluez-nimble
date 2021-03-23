#include "blescan.h"

static void sigint_handler(int sig){
	signal_received = sig;
}


int strcmpi(char* a, char* b){
	int ptr = 0;
	while (a[ptr] != '\0' && b[ptr] != '\0'){
		if (a[ptr] >= 'A' && a[ptr] <= 'Z'){
			a[ptr] += 'a' - 'A';
		}
		if (b[ptr] >= 'A' && b[ptr] <= 'Z'){
			b[ptr] += 'a' - 'A';
		}
		if (a[ptr] != b[ptr]){
			return ptr;
		}
		ptr++;
	}
	return -1;
}



static void eir_parse_name(uint8_t *eir, size_t eir_len,
						char *buf, size_t buf_len)
{
	size_t offset;

	offset = 0;
	while (offset < eir_len) {
		uint8_t field_len = eir[0];
		size_t name_len;

		/* Check for the end of EIR */
		if (field_len == 0)
			break;

		if (offset + field_len > eir_len)
			goto failed;

		switch (eir[1]) {
		case EIR_NAME_SHORT:
		case EIR_NAME_COMPLETE:
			name_len = field_len - 1;
			if (name_len > buf_len)
				goto failed;

			memcpy(buf, &eir[2], name_len);
			return;
		}

		offset += field_len + 1;
		eir += field_len + 1;
	}

failed:
	snprintf(buf, buf_len, "(unknown)");
}

int blescan(int id_type, char *id, int timeout, char *dev_addr){
	int dev_id, sock;
  uint8_t own_type = 0x00;
	uint8_t scan_type = 0x01;
	uint8_t filter_type = 0;
	uint8_t filter_policy = 0x00;
	uint16_t interval = htobs(0x0010);
	uint16_t window = htobs(0x0010);
	uint8_t filter_dup = 1;
	socklen_t filter_size;
	struct hci_filter current_filter, last_filter;
	struct sigaction sa;
	int err;
	char name[30], addr[18];
	unsigned char hci_read_buf[HCI_MAX_EVENT_SIZE], *hci_ptr;

  dev_id = hci_get_route(NULL);  // 找到默认蓝牙设备的编号
  sock = hci_open_dev(dev_id);  // 打开与ble的sock
  if (dev_id < 0 || sock < 0){
		perror("device not find");
		goto done;
  }
  err = hci_le_set_scan_parameters(sock, scan_type, interval, window, own_type, filter_type, 1000);
  if (err < 0){
		perror("le scan parameters set error");
		goto done;
  }
  err = hci_le_set_scan_enable(sock, 1, filter_dup, 1000);
  if (err < 0){
		perror("le scan enable error");
		goto done;
  }
  // 设置事件过滤器
  filter_size = sizeof(current_filter);
  hci_filter_clear(&current_filter);
  hci_filter_set_ptype(HCI_EVENT_PKT, &current_filter);
  hci_filter_set_event(EVT_LE_META_EVENT, &current_filter);
  err = getsockopt(sock, SOL_HCI, HCI_FILTER, &last_filter, &filter_size);
  if (err < 0){
		perror("get sock opt error");
		goto done;
  }
  err = setsockopt(sock, SOL_HCI, HCI_FILTER, &current_filter, sizeof(current_filter));

  memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_NOCLDSTOP;
	sa.sa_handler = sigint_handler;
	sigaction(SIGINT, &sa, NULL);
	int len = 0;
	// 对HCI sock进行读取
	while (timeout > 0){
		evt_le_meta_event *meta;
		le_advertising_info *info;
		while ((len = read(sock, hci_read_buf, sizeof(hci_read_buf))) < 0){
			if (errno == EINTR && signal_received == SIGINT){
//			if (errno == EINTR){
				len = 0;
				goto done;
			}
			if (errno == EAGAIN || errno == EINTR){
				continue;
			}
			goto done;
		}
		hci_ptr = hci_read_buf + (1 + HCI_EVENT_HDR_SIZE);
		len -= (1 + HCI_EVENT_HDR_SIZE);
		meta = (void*) hci_ptr;
		if (meta->subevent != 0x02){  // BT_HCI_EVT_LE_ADV_REPORT bt.h Line 2327
			goto done;
		}
		info = (le_advertising_info *) (meta->data + 1);
		memset(addr, 0, sizeof(addr));
		memset(name, 0, sizeof(name));
		ba2str(&info->bdaddr, addr);
		eir_parse_name(info->data, info->length, name, sizeof(name) - 1);
		if (id_type == ID_TYPE_MAC){
			if (strcmpi(id, addr) == -1){
				setsockopt(sock, SOL_HCI, HCI_FILTER, &last_filter, sizeof(last_filter));
				hci_le_set_scan_enable(sock, 0x00, 1, 1000);
				hci_close_dev(dev_id);
				memcpy(dev_addr, addr, sizeof(addr));
				return 1;
			}
			else{
				timeout--;
			}
		}
		else if (id_type == ID_TYPE_NAME){
			if (strcmp(id, name) == 0){
				printf("%s  %s\n", addr, name);
				setsockopt(sock, SOL_HCI, HCI_FILTER, &last_filter, sizeof(last_filter));
				hci_le_set_scan_enable(sock, 0x00, 1, 1000);
				hci_close_dev(dev_id);
				memcpy(dev_addr, addr, sizeof(dev_addr));
				return 1;
			}
			else{
				timeout--;
			}
		}
		else{
			perror("illegal ID type");
			return 0;
		}
	}

done:
	setsockopt(sock, SOL_HCI, HCI_FILTER, &last_filter, sizeof(last_filter));
	hci_le_set_scan_enable(sock, 0x00, 1, 1000);
	hci_close_dev(dev_id);
	return 0;
}
