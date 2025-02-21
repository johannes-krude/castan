#include <stdio.h>
#include <sys/time.h>

#include <netinet/in.h>

typedef unsigned int data_t;

void lpm_init();
void lpm_set_prefix_data(struct in_addr *ip, int prefix_len, data_t data);
data_t lpm_get_ip_data(struct in_addr *ip);
