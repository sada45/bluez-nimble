# bluez-nimble
use bluez stack to connect with nimble stack. Dose not work well yet.

1.use `make` to make the l2cap_client

2.burn the [nimble_l2cap_server](https://github.com/RIOT-OS/RIOT/tree/master/tests/nimble_l2cap_server)

3.run the l2cap_client with ythe node's MAC address
```
sudo ./l2cap_client -m <MAC address>
```
