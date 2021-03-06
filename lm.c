#include "lm.h"


uint8_t * addr2arr(struct in_addr ipadrr){
	static uint8_t iparr[4] = {0};
	char *ipstr = inet_ntoa(ipadrr);
	sscanf(ipstr, "%hhu.%hhu.%hhu.%hhu", &iparr[0], &iparr[1], &iparr[2], &iparr[3]);
	return iparr;
}

uint8_t addr2size(struct in_addr ipadrr){
	static uint8_t iparr[4] = {0};
	char *ipstr = inet_ntoa(ipadrr);
	sscanf(ipstr, "%hhu.%hhu.%hhu.%hhu", &iparr[0], &iparr[1], &iparr[2], &iparr[3]);
	uint8_t sum = 0;
	sum = ndigits(iparr[0])+ndigits(iparr[1])+ndigits(iparr[2])+ndigits(iparr[3]);
	return sum;
}

void clearscr(){
	printf("\033[2J\x1b[H");
}

void fill(int n){
	int mode = 3;
	int i = 0;
	for(i=0; i < n; i++){
		switch(mode){
			case 0:
				printf("|");
				break;
			case 1:
				printf("░");
				break;
			case 2:
				printf("▒");
				break;
			case 3:
				printf("▓");
				break;
			case 4:
				printf("█");
				break;
			case 5:
				printf("■");
				break;

		}

	}
}

void empty(int n){
	int i = 0;
	for(i=0; i < n; i++){
		printf(" ");
	}
}

int config_serial (int fd, int speed, int parity){
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                perror("error %d from tcgetattr");
                return -1;
        }

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
                perror("error %d from tcsetattr");
                return -1;
        }
        return 0;
}

void set_blocking (int fd, int should_block){
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                perror ("error %d from tggetattr");
                return;
        }

        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
                perror ("error %d setting term attributes");
}


void print_help(){
	printf("\n oled4linux: Send monitorization info to an external oled screen via serial\n");
		printf("\n OPTIONS:\n");
		printf("\t -h \tPrint this help\n");
		printf("\t -t [TIME]\tSend time.\n");
		printf("\t -s [SERIAL_PATH]\tSpecify a serial port to send the data\n");
		printf("\t -n [port]\tSend data using udp to the specified port.\n");
		printf("\t -c \tShow the data using ncurses style\n");


	exit(1);
}

uint8_t ndigits(int n){
	uint8_t count = 0;
    while(n != 0){
        n /= 10;
        ++count;
    }
    if(count==0) count = 1;
	return count;
}


int main(int argc, char *argv[]){
	unsigned int laptime = DEFAULT_DELAY;
	uint8_t outopt = 0;
	uint32_t port = DEFAULT_PORT;
	int opt;
	char dev[32];
	char dev2[32];
	char *serialpath;
	int fd;

	uint32_t cpufull[64];
	uint32_t cpuidle[64];
	uint32_t last_cpufull[64];
	uint32_t last_cpuidle[64];

	txrx_t if1stats;
	txrx_t if2stats;
	if1stats.tx = 0;
	if1stats.rx = 0;
	if1stats.lasttx = 0;
	if1stats.lastrx = 0;
	if2stats.tx = 0;
	if2stats.rx = 0;
	if2stats.lasttx = 0;
	if2stats.lastrx = 0;

	int cores = 0;

	time_t rawtime;
	struct tm * timeinfo;

	ram_t raminfo;
	uptime_t upt;

	double load[3];

	struct in_addr addr = { 0 };
	struct in_addr addr2 = { 0 };

	swap_t swap;
	swap_t mem;

	int i = 0;
	for (i=0; i<64; ++i){
		cpufull[i] = 0;
		cpuidle[i] = 0;
		last_cpuidle[i] = 0;
		last_cpufull[i] = 0;
	}

	int ifaces = 0;

	while ((opt = getopt(argc, argv, "s:t:n:hc")) != -1) {
		switch(opt) {
			case 's':
				outopt = OUTPUT_SERIAL;
				serialpath = optarg;
				break;
			case 't':
				laptime = atoi(optarg);
				break;
			case 'h':
				print_help();
				break;
			case 'n':
				outopt= OUTPUT_NETWORK;
				port = atoi(optarg);
			case 'c':
				outopt = OUTPUT_CONSOLE;
		}
	}
	if(outopt == OUTPUT_SERIAL){
		fd = open (serialpath, O_RDWR | O_NOCTTY | O_SYNC);
		if (fd < 0){
			perror("error opening serial conn");
			outopt = OUTPUT_CONSOLE;
		}

		config_serial (fd, SERIAL_SPEED, 0);  // set speed to 115,200 bps, 8n1 (no parity)
		set_blocking (fd, 0);                // set no blocking
	}
	if (outopt == OUTPUT_NETWORK){

	}
	int n = 0;
	while(1){

		raminfo = get_ram();
		upt = get_uptime();

		if (getloadavg(load, 3) == -1){
			load[0] = 0;
			load[1] = 0;
			load[2] = 0;
		}

		time(&rawtime);
		timeinfo = localtime(&rawtime);

		if(n%10==0){
			ifaces = get_ifname(dev,1);
			addr = get_addr(dev);

			if (ifaces > 1){
					get_ifname(dev2,2);
					addr2 = get_addr(dev2);
			}

			mem = get_disk_bymnt("/");
		}
		if(n%2){
			swap = get_swap();
		}
		gettxrx(dev,&if1stats);
		gettxrx(dev2,&if2stats);



		memcpy(last_cpufull, cpufull, sizeof(cpufull));
		memcpy(last_cpuidle, cpuidle, sizeof(cpuidle));
		cores = get_cpu(cpufull,cpuidle);

		if(outopt == OUTPUT_SERIAL){
			serial_pkt serialbuff;
			serialbuff.ramtotal = raminfo.total/1024;
			serialbuff.ramfree = raminfo.used/1024;
			serialbuff.upt_days = upt.days;
			serialbuff.upt_hours = upt.hours;
			serialbuff.upt_mins = upt.mins;
			serialbuff.upt_secs = upt.secs;
			serialbuff.load[0] = load[0]*100;
			serialbuff.load[1] = load[1]*100;
			serialbuff.load[2] = load[2]*100;
			serialbuff.sec = timeinfo->tm_sec;
			serialbuff.min = timeinfo->tm_min;
			serialbuff.hour = timeinfo->tm_hour;
			serialbuff.day = timeinfo->tm_mday;
			serialbuff.month = timeinfo->tm_mon + 1;
			serialbuff.year = timeinfo->tm_year - 100;
			uint8_t *iparr = addr2arr(addr);
			serialbuff.ip[0] = iparr[0];
			serialbuff.ip[1] = iparr[1];
			serialbuff.ip[2] = iparr[2];
			serialbuff.ip[3] = iparr[3];
			write (fd, &serialbuff, sizeof(serial_pkt));
			//usleep ((7 + 25) * 100);
			// receive 25:  approx 100 uS per char transmit
		}
		if(outopt == OUTPUT_NETWORK){

		}

		if(outopt == OUTPUT_CONSOLE){
			struct winsize w;
			ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
			// printf("%05d", zipCode);
			//printf ("lines %d columns %d\n", w.ws_row,  w.ws_col);

		// http://www.isthe.com/chongo/tech/comp/ansi_escapes.html
		clearscr();
		time_bar(timeinfo,upt,w.ws_col);
		status_bar(load,w.ws_col);

		cpu_bar(cores,cpufull,cpuidle,last_cpufull,last_cpuidle,w.ws_col);

		ram_bar(raminfo,w.ws_col);
		swap_bar(swap,w.ws_col);

		storage_bar(mem, w.ws_col);

		printf("\n");
		ip_bar(dev,dev2,addr,addr2,ifaces,&if1stats,&if2stats,w.ws_col);


		}
		n ++;
		if (n==101) n = 0;
		sleep(laptime);
	}
	return 0;

}
