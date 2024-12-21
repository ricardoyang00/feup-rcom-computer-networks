# Step 1 Cabos
- tux42 eth1 --> 2
- tux43 eth1 --> 3
- tux44 eth1 --> 4
- tux44 eth2 --> 8
- RC (Router) eth1 --> P4.12
- RC (Router) eth2 --> 12
- Mikrotik Switch to `tux43 S0`

# Step 2 Restart

1. **`systemctl restart networking`** in all tux

2. **`system reset-config`** in GTK (port 11520) in tux43

# Step 3 Config eth

1. **`ifconfig eth1 up`** in tux43

2. **`ifconfig eth1 172.16.40.1/24`** in tux43

3. In gtk 
    - CREATE BRIDGES
        - **`/interface bridge add name=bridge40`**
        - **`/interface bridge add name=bridge41`**

    - REMOVE PORT FROM BRIDGES
        - **`/interface bridge port remove [find interface=ether2]`**
        - **`/interface bridge port remove [find interface=ether3]`**
        - **`/interface bridge port remove [find interface=ether4]`**
        - **`/interface bridge port remove [find interface=ether8]`**
        - **`/interface bridge port remove [find interface=ether12]`**

    - ADD PORT TO BRIDGES
        - **`/interface bridge port add bridge=bridge40 interface=ether3`**
        - **`/interface bridge port add bridge=bridge40 interface=ether4`**
        - **`/interface bridge port add bridge=bridge41 interface=ether2`**
        - **`/interface bridge port add bridge=bridge41 interface=ether8`**
        - **`/interface bridge port add bridge=bridge41 interface=ether12`**

3. TUX44
    - **`ifconfig eth1 up`**
    - **`ifconfig eth2 up`**
    - **`ifconfig eth1 172.16.40.254/24`**
    - **`ifconfig eth2 172.16.41.253/24`**
    - **`sysctl net.ipv4.ip_forward=1`**
    - **`sysctl net.ipv4.icmp_echo_ignore_broadcasts=0`**


4. TUX42
    - **`ifconfig eth1 up`**
    - **`ifconfig eth1 172.16.41.1/24`**

# Step 4 Config RC

1. 
    - Mudar cabo para MT (em cima do branco)
    - **`systen reset-config`** in Mikrotik Router

2. GTKterm Config RC IP
    - **`/ip address add address=172.16.1.41/24 interface=ether1`**
    - **`/ip address add address=172.16.41.254/24 interface=ether2`**
    
# Step 5 Add routes

1. GTKterm Add RC route
    - **`/ip route add dst-address=172.16.40.0/24 gateway=172.16.41.253`**

2. TUX43
    - **`route add -net 172.16.41.0/24 gw 172.16.40.254`**
    - **`route add -net 172.16.1.0/24 gw 172.16.40.254`**

3. TUX44
    - **`route add -net 172.16.1.0/24 gw 172.16.41.254`**

4. TUX42
    - **`route add -net 172.16.40.0/24 gw 172.16.41.253`**
    - **`route add -net 172.16.1.0/24 gw 172.16.41.254`**
