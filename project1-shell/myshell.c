#include <stdio.h>//��׼I/O��
#include <stdlib.h>//ʵ�ó���⺯��
#include <string.h>//�ַ�������
#include <signal.h>// �ź�
#include <curses.h>
#include <limits.h>//ʵ�ֳ���
#include <termcap.h>
#include <termios.h>
#include <time.h>//ʱ�������
#include <errno.h>//������
#include <assert.h>//��֤�������

#include <dirent.h>//Ŀ¼��
#include <fcntl.h>//�ļ�����
#include <pwd.h>//�����ļ�
#include <unistd.h>//���ų���

#include <minix/com.h>
#include <minix/config.h>
#include <minix/type.h>
#include <minix/endpoint.h>
#include <minix/const.h>
#include <minix/u64.h>
#include <paths.h>
#include <minix/procfs.h>

#include <sys/stat.h>//�ļ�״̬
#include <sys/types.h>//����ϵͳ��������
#include <sys/wait.h>//���̿���
#include <sys/times.h>//����ʱ��
#include <sys/time.h>
#include <sys/select.h>//select����


//һ�������0
//����ض���1
//�����ض���2
//�������йܵ�3
//#define SELF	((endpoint_t) 0x8ace) 	/* used to indicate 'own process' */
//#define _MAX_MAGIC_PROC (SELF)	/* used by <minix/endpoint.h> */
//#define MAX_NR_TASKS	1023
//#define _ENDPOINT_GENERATION_SIZE (MAX_NR_TASKS+_MAX_MAGIC_PROC+1)
//#define _ENDPOINT_P(e)
	//((((e)+1023) % (1023+((endpoint_t) 0x8ace)+1)) - 1023)
//#define  SLOT_NR(e) (_ENDPOINT_P(e) + nr_tasks)

#define _PATH_PROC "/proc/"
#define PSINFO_VERSION	0
#define TYPE_TASK	'T'
#define TYPE_SYSTEM	'S'
#define TYPE_USER	'U'
#define STATE_RUN	'R'
#define TIMECYCLEKEY 't'
#define ORDERKEY 'o'

#define  USED		0x1
#define  IS_TASK	0x2
#define  IS_SYSTEM	0x4
#define  BLOCKED	0x8

#define ORDER_CPU	0
#define ORDER_MEMORY	1
#define ORDER_HIGHEST	ORDER_MEMORY

#define  TC_BUFFER  1024        /* Size of termcap(3) buffer    */
#define  TC_STRINGS  200        /* Enough room for cm,cl,so,se  */


/* name of cpu cycle types, in the order they appear in /psinfo. */
const char *cputimenames[] = { "user", "ipc", "kernelcall" };
#define CPUTIMENAMES (sizeof(cputimenames)/sizeof(cputimenames[0]))

#define CPUTIME(m, i) (m & (1L << (i)))

struct proc {
	int p_flags;//
	endpoint_t p_endpoint;//�˵�
	pid_t p_pid;//���̺�
	u64_t p_cpucycles[CPUTIMENAMES];//cpu����
	int p_priority; //��̬���ȼ�
	endpoint_t p_blocked; //����״̬
	time_t p_user_time; //�û�ʱ��
	vir_bytes p_memory; //�ڴ�
	uid_t p_effuid;//��Ч�û�ID
	int p_nice; //��̬���ȼ�
	char p_name[PROC_NAME_LEN+1]; //����
};

struct proc *proc = NULL, *prev_proc = NULL;

struct tp {
	struct proc *p;
	u64_t ticks;
};

void getOrder(char *);     //�õ����������
void anaOrder(char *, int *, char (*)[256]);  //�������������н���
void exeCommand(int, char (*)[256]);   //ִ������
int checkCommand(char *command);   //���������еĿ�ִ�г���
void getkinfo();//��ȡ/proc/kinfo�õ��ܵĽ��̺�������nr_total
int print_memory();//����ڴ���Ϣ
void get_procs(); //��¼��ǰ���̣���ֵ��prev_proc
void parse_dir();//��ȡ��/proc/�µ����н���pid
void parse_file(pid_t pid);//��ȡÿһ��������Ϣ
u64_t cputicks(struct proc *p1, struct proc *p2, int timemode);//����ÿ�����̵ĵδ�
void print_procs(struct proc *proc1, struct proc *proc2, int cputimemode);//���CPUʹ��ʱ��

char *message;  //����myshell��ʾ��Ϣ�����
int whilecnt=0;
char msg[40][256];
unsigned int nr_procs, nr_tasks;//��������������
int nr_total;//���̺���������
int order = ORDER_CPU;
int blockedverbose = 0;
char *Tclr_all;
int slot=1;

/*static inline u64_t make64(unsigned long lo, unsigned long hi)
{
	return ((u64_t)hi << 32) | (u64_t)lo;
}
*/


/*
������: memset
��  ��: ����s�е������ֽ�Ϊch, s����Ĵ�С��n����
��  ��: void *memset(void *s, char ch, unsigned n);
������:

#include <string.h>
#include <stdio.h>
#include <mem.h>

int main(void)
{
   char buffer[] = "Hello world\n";

   printf("Buffer before memset: %s\n", buffer);
   memset(buffer, '*', strlen(buffer) - 1);
   printf("Buffer after memset:  %s\n", buffer);
   return 0;
}
*/
/*
������: getcwd
��  ��: ȡ��ǰ����Ŀ¼
��  ��: char *getcwd(char *buf, int n);
������:
#include <stdio.h>
#include <dir.h>
int main(void)
{
   char buffer[MAXPATH];

   getcwd(buffer, MAXPATH);
   printf("The current directory is: %s\n", buffer);
   return 0;
}
*/

int main(int argc, char **argv)
{
    int i;
    int wordcount = 0;
    char wordmatrix[100][256];
    char **arg = NULL;
    char *buf = NULL; //�û�����

    if((buf = (char *)malloc(256))== NULL){
        perror("malloc failed");
        exit(-1);
    }

    while(1){
        memset(buf, 0, 256);    //��buf��ָ�Ŀռ�����
        message = (char *)malloc(100);
        getcwd(message, 100);
        printf("myshell:%s# ", message);
        free(message);
        getOrder(buf);
        strcpy(msg[whilecnt],buf);
        whilecnt++;
        if( strcmp(buf, "exit\n") == 0 ){
            break;
        }
        for(i = 0; i < 100; i++){
            wordmatrix[i][0] = '\0';
        }
        wordcount = 0;
        anaOrder(buf, &wordcount, wordmatrix);
        exeCommand(wordcount, wordmatrix);
    }

    if(buf != NULL){
        free(buf);
        buf = NULL;
    }
    return 0;
}


//��ȡ�û�����
void getOrder(char *buf){
    int cnt = 0;

    int c = getchar();
    while(cnt < 256 && c != '\n'){
        buf[cnt++] = c;
        c = getchar();
    }
    buf[cnt++]='\n';
    buf[cnt]='\0';
    if(cnt == 256){
        exit(-1);
    }
}

//����buf�е�������������wordmatrix�У������Իس�����\n����
void anaOrder(char* buf, int* wordcount, char (*wordmatrix)[256]){
  char *p = buf;
  char *q = buf;
  int number = 0;
  int i;
  while(1){
      if(p[0] == '\n'){
          break;
      }
			if(p[0] == ' '){
          p++;
      }
      else{
          q = p;
          number = 0;
          while((q[0] != ' ') && (q[0] != '\n')){
              if(q[0] == 92){
                  q[0] = ' ';
                  q[1] = q[2];
                  for(i = 2; ; i++){
                      q[i] = q[i+1];
                      if((q[i] == ' ') || (q[i] == '\n'))
                          break;
                  }
              }
              number++;
              q++;
          }
          strncpy(wordmatrix[*wordcount], p, number + 1);
          wordmatrix[*wordcount][number] = '\0';
          *wordcount = *wordcount + 1;
          p = q;
      }
  }
}
/*
������: dup2
��  ��: �����ļ����
��  ��: int dup2(int oldhandle, int newhandle);
������:

#include <sys\stat.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>

int main(void)
{
   #define STDOUT 1

   int nul, oldstdout;
   char msg[] = "This is a test";

    //create a file
   nul = open("DUMMY.FIL", O_CREAT | O_RDWR,
      S_IREAD | S_IWRITE);

   //create a duplicate handle for standard output
   oldstdout = dup(STDOUT);
      redirect standard output to DUMMY.FIL
      by duplicating the file handle onto the
      file handle for standard output.
   dup2(nul, STDOUT);

    close the handle for DUMMY.FIL
   close(nul);

   will be redirected into DUMMY.FIL
   write(STDOUT, msg, strlen(msg));

    restore original standard output handle
   dup2(oldstdout, STDOUT);

   //close duplicate handle for STDOUT
   close(oldstdout);

   return 0;
}
*/


void exeCommand(int wordcount, char (*wordmatrix)[256]){
    pid_t pid;
    char *file;
    int correct = 1;
    int way = 0;
    int status;
    int i;
    int fd;
    char *word[wordcount + 1];
    char *wordnext[wordcount + 1];

    //������ȡ��
    for(i = 0; i < wordcount; i++){
        word[i] = (char *)wordmatrix[i];
    }
    word[wordcount] = NULL;

    //�鿴�������Ƿ��к�̨���з�
    for(i = 0; i < wordcount; i++){
        if(strncmp(word[i], "&", 1) == 0){
            way=4;
            word[wordcount - 1] = NULL;
        }
    }

    for(i = 0; word[i] != NULL; i++){
        if(strcmp(word[i], ">") == 0){
            way = 1;
            if(word[i + 1] == NULL){
                correct=0;
            }
        }
        if(strcmp(word[i], "<") == 0){
            way = 2;
            if(i == 0){
                correct=0;
            }
        }
        if(strcmp(word[i], "|") == 0){
            way = 3;
            if(word[i+1] == NULL){
                correct=0;
            }
            if(i == 0){
                correct=0;
            }
        }
    }
    //correct=0,˵�������ʽ����
    if(correct==0){
        printf("wrong format\n");
        return ;
    }

    if(way == 1){
        //����ض���
        for(i = 0; word[i] != NULL; i++){
            if(strcmp(word[i], ">") == 0){
                file = word[i+1];
                word[i] = NULL;
            }
        }
    }

    if(way == 2){
        //�����ض���
        for(i = 0; word[i] != NULL; i++){
            if(strcmp(word[i], "<") == 0){
                file = word[i + 1];
                word[i] = NULL;
            }
        }
    }

    if(way == 3){
        //�ѹܵ�������Ĳ��ִ���wordnext�У��ܵ�����Ĳ�����һ����ִ�е�shell����
        for(i = 0; word[i] != NULL; i++){
            if(strcmp(word[i], "|") == 0){
                word[i] = NULL;
                int j;
                for(j = i + 1; word[j] != NULL; j++){
                    wordnext[j-i-1] = word[j];
                }
                wordnext[j-i-1] = NULL;
            }
        }
    }

    if(strcmp(word[0], "cd") == 0){
        if(word[1] == NULL){
            return ;
        }
        if(chdir(word[1]) ==  -1){
            perror("cd");
        }
        return ;
    }



    if((pid = fork()) < 0){  //�����ӽ���
        printf("fork error\n");
        return ;
    }

    if(strcmp(word[0], "history") == 0){  //ʵ��history n
        if(pid == 0){
            char cnum=word[1][0];
            int num=cnum-48;
            for(int i=whilecnt-1;i>whilecnt-num-1;i--){
                printf("command %d:",i);
                printf("%s",msg[i]);
            }
            exit(0);
        }

    }

		if(strcmp(word[0], "mytop") == 0){
        if(pid == 0){
					  int cputimemode = 1;
            getkinfo();
						print_memory();
						get_procs();
						if (prev_proc == NULL)
							get_procs();
						print_procs(prev_proc, proc, cputimemode);
						exit(0);
            }
      }

    switch(way){
    case 0:
        //pidΪ0˵�����ӽ��̣����ӽ�����ִ�����������
        //����������в���> < | &
        if(pid == 0){
            if(!(checkCommand(word[0]))){
                printf("%s : command not found\n", word[0]);
                exit(0);
            }
            execvp(word[0], word);
            exit(0);
        }
        break;

    case 1:
        //����������к�������ض����
        if(pid == 0){
            if( !(checkCommand(word[0])) ){
                printf("%s : command not found\n", word[0]);
                exit(0);
            }
            fd = open(file, O_RDWR|O_CREAT|O_TRUNC, 0644);
            dup2(fd, 1);
            execvp(word[0], word);
            exit(0);
        }
        break;

    case 2:
        //����������к��������ض���<
        if(pid == 0){
            if( !(checkCommand (word[0])) ){
                printf("%s : command not found\n", word[0]);
                exit(0);
            }
            fd = open(file, O_RDONLY);
            dup2(fd, 0);
            execvp(word[0], word);
            exit(0);
        }
        break;

    case 3:
        //����������к��йܵ���|
        if(pid == 0){
            int pid2;
            int status2;
						int fd[2];

						if(pipe(fd)<0){
							printf("pipe error\n");
							exit(0);
						}

            if( (pid2 = fork()) < 0 ){
                printf("fork2 error\n");
                return ;
            }
            else if(pid2 == 0){
                if( !(checkCommand(word[0])) ){
                    printf("%s : command not found\n", word[0]);
                    exit(0);
                }
                dup2(fd[1], 1);
                execvp(word[0], word);
                exit(0);
            }
            if(waitpid(pid2, &status2, 0) == -1){
                printf("wait for child process error\n");
            }
            if( !(checkCommand(wordnext[0])) ){
                printf("%s : command not found\n", wordnext[0]);
                exit(0);
            }
            dup2(fd[0], 0);
            execvp (wordnext[0], wordnext);
            exit(0);
        }
        break;
		case 4://����������&����ʾ��ִ̨�У�������ֱ�ӷ��أ����ȴ��ӽ��̽���
		  signal(SIGCHLD,SIG_IGN);
			if(pid==0){
				//int pidbg;
				//pidbg=fork();
				//if(pidbg==0){
					printf("[process id: %d]\n",pid);
					int a = open("/dev/null",O_RDONLY);
					dup2(a,0);
					dup2(a,1);
					if( !(checkCommand(word[0])) ){
							printf("%s : command not found\n", word[0]);
							exit(0);
					}
					execvp(word[0],word);
					exit(0);
				//}
		 }
		 break;


    default:
        break;
    }

    //�����̵ȴ��ӽ��̽���
    if(waitpid(pid, &status, 0) == -1){
        printf("wait for child process error\n");
    }
}

//���������еĿ�ִ�г���
int checkCommand(char *command){
    DIR *dp;
    struct dirent *dirp;
    char *path[] = {"./", "/bin", "/usr/bin", NULL};

    if( strncmp(command, "./", 2) == 0 )
    {
        command = command + 2;
    }

    //�ֱ��ڵ�ǰĿ¼��/bin��/usr/binĿ¼����Ҫִ�еĳ���
    int i = 0;
    while(path[i] != NULL)
    {
        if( (dp= opendir(path[i])) ==NULL )
        {
            printf("can not open /bin \n");
        }
        while( (dirp = readdir(dp)) != NULL )
        {
            if(strcmp(dirp->d_name, command) == 0)
            {
                closedir(dp);
                return 1;
            }
        }
        closedir(dp);
        i++;
    }
    return 0;
}

void getkinfo(){
	FILE *fp;

  if ((fp = fopen("/proc/kinfo", "r")) == NULL) {
		fprintf(stderr, "opening " _PATH_PROC "kinfo failed\n");
		exit(1);
	}

	if (fscanf(fp, "%u %u", &nr_procs, &nr_tasks) != 2) {
		fprintf(stderr, "reading from " _PATH_PROC "kinfo failed\n");
		exit(1);
	}

	fclose(fp);

	nr_total = (int) (nr_procs + nr_tasks);
}

int print_memory()
{
	FILE *fp;
	unsigned int pagesize;
	unsigned long total, free, largest, cached;

	if ((fp = fopen("/proc/meminfo", "r")) == NULL)
		return 0;

	if (fscanf(fp, "%u %lu %lu %lu %lu", &pagesize, &total, &free,
			&largest, &cached) != 5) {
		fclose(fp);
		return 0;
	}

	fclose(fp);

	printf("main memory: %ldK total, %ldK free, %ldK contig free, "
		"%ldK cached\n",
		(pagesize * total)/1024, (pagesize * free)/1024,
		(pagesize * largest)/1024, (pagesize * cached)/1024);

	return 1;
}

void get_procs(){
	struct proc *p;
	int i;

	p = prev_proc;
	prev_proc = proc;
	proc = p;

	if (proc == NULL) {
		proc = malloc(nr_total * sizeof(proc[0]));

		if (proc == NULL) {
			fprintf(stderr, "Out of memory!\n");
			exit(1);
		}
	}

	for (i = 0; i < nr_total; i++)
		proc[i].p_flags = 0;

	parse_dir();
}


void parse_dir(){
	DIR *p_dir;
	struct dirent *p_ent;
	pid_t pid;
	char *end;//end��ָ���һ������ת�����ַ�λ�õ�ָ��

	if ((p_dir = opendir("/proc")) == NULL) {
		perror("opendir on " _PATH_PROC);
		exit(1);
	}

	for (p_ent = readdir(p_dir); p_ent != NULL; p_ent = readdir(p_dir)) {
		pid = strtol(p_ent->d_name, &end, 10);//long strtol(char *str, char **endptr, int base)����ת��Ϊ������,base�ǻ�������ʾҪת�����Ǽ����Ƶ���

		if (!end[0] && pid != 0)
			parse_file(pid);
	}

	closedir(p_dir);
}


void parse_file(pid_t pid)
{
	char path[PATH_MAX], name[256], type, state;
	int version, endpt, effuid; //�汾���˵㣬��Ч�û�ID
	unsigned long cycles_hi, cycles_lo; //�����ڣ�������
	FILE *fp;
	struct proc *p;
	//int slot; //���?
	int i;
	//printf("parse_file\n");

	sprintf(path, "/proc/%d/psinfo", pid);

	if ((fp = fopen(path, "r")) == NULL)
		return;

	if (fscanf(fp, "%d", &version) != 1) {
		fclose(fp);
		return;
	}

	if (version != PSINFO_VERSION) { //0
		fputs("procfs version mismatch!\n", stderr);
		exit(1);
	}

	if (fscanf(fp, " %c %d", &type, &endpt) != 2) {
		fclose(fp);
		return;
	}

	slot++;
	//slot = SLOT_NR(endpt);

	if(slot < 0 || slot >= nr_total) {
		fprintf(stderr, "top: unreasonable endpoint number %d\n", endpt);
		fclose(fp);
		return;
	}

	p = &proc[slot];

	if (type == TYPE_TASK)
		p->p_flags |= IS_TASK;
	else if (type == TYPE_SYSTEM)
		p->p_flags |= IS_SYSTEM;

	p->p_endpoint = endpt;
	p->p_pid = pid;

	if (fscanf(fp, " %255s %c %d %d %lu %*u %lu %lu",
		name, &state, &p->p_blocked, &p->p_priority,
		&p->p_user_time, &cycles_hi, &cycles_lo) != 7) {

		fclose(fp);
		return;
	}

	strncpy(p->p_name, name, sizeof(p->p_name)-1);
	p->p_name[sizeof(p->p_name)-1] = 0;

	if (state != STATE_RUN)
		p->p_flags |= BLOCKED;
	p->p_cpucycles[0] = make64(cycles_lo, cycles_hi);
	p->p_memory = 0L;

	if (!(p->p_flags & IS_TASK)) {
		int j;
		if ((j=fscanf(fp, " %lu %*u %*u %*c %*d %*u %u %*u %d %*c %*d %*u",
			&p->p_memory, &effuid, &p->p_nice)) != 3) {

			fclose(fp);
			return;
		}

		p->p_effuid = effuid;
	} else p->p_effuid = 0;

	for(i = 1; i < CPUTIMENAMES; i++) {
		if(fscanf(fp, " %lu %lu",
			&cycles_hi, &cycles_lo) == 2) {
			p->p_cpucycles[i] = make64(cycles_lo, cycles_hi);
		} else	{
			p->p_cpucycles[i] = 0;
		}
	}

	if ((p->p_flags & IS_TASK)) {
		if(fscanf(fp, " %lu", &p->p_memory) != 1) {
			p->p_memory = 0;
		}
	}

	p->p_flags |= USED;

	fclose(fp);
}

u64_t cputicks(struct proc *p1, struct proc *p2, int timemode)
{
	int i;
	u64_t t = 0;
	for(i = 0; i < CPUTIMENAMES; i++) {
		if(!CPUTIME(timemode, i))
			continue;
		if(p1->p_endpoint == p2->p_endpoint) {
			t = t + p2->p_cpucycles[i] - p1->p_cpucycles[i];
		} else {
			t = t + p2->p_cpucycles[i];
		}
	}

	return t;
}


void print_procs(struct proc *proc1, struct proc *proc2, int cputimemode)
{
	int p, nprocs;
	//u64_t idleticks = 0;
	//u64_t kernelticks = 0;
	u64_t systemticks = 0;
	u64_t userticks = 0;
	u64_t total_ticks = 0;
	int blockedseen = 0;
	static struct tp *tick_procs = NULL;//tp�ṹ�������tick_procs,�����еĽ��̺�����(�������������nr_total)����ticks

	if (tick_procs == NULL) {
		tick_procs = malloc(nr_total * sizeof(tick_procs[0]));

		if (tick_procs == NULL) {
			fprintf(stderr, "Out of memory!\n");
			exit(1);
		}
	}

	for(p = nprocs = 0; p < nr_total; p++) {
		u64_t uticks;
		if(!(proc2[p].p_flags & USED))
			continue;
		tick_procs[nprocs].p = proc2 + p;
		tick_procs[nprocs].ticks = cputicks(&proc1[p], &proc2[p], cputimemode);
		uticks = cputicks(&proc1[p], &proc2[p], 1);
		total_ticks = total_ticks + uticks;
		//printf("total_ticks:%llu\n",total_ticks);
		/*if(p-NR_TASKS == IDLE) {
			idleticks = uticks;
			continue;
		}
		if(p-NR_TASKS == KERNEL) {
			kernelticks = uticks;
		}
		*/
		if(!(proc2[p].p_flags & IS_TASK)) {
		 if(proc2[p].p_flags & IS_SYSTEM)
			 systemticks = systemticks + tick_procs[nprocs].ticks;
		 else
			 userticks = userticks + tick_procs[nprocs].ticks;
	 }

		nprocs++;
	}

	if (total_ticks == 0)
		return;


	printf("CPU states: %6.2f%% user, ", 100.0 * userticks / total_ticks);
	printf("%6.2f%% system", 100.0 * systemticks / total_ticks);
	printf("%6.2f%% in total\n", 100.0 * (systemticks+userticks) / total_ticks);
	//printf("%6.2f%% kernel, ", 100.0 * kernelticks/ total_ticks);
	//printf("%6.2f%% idle", 100.0 * idleticks / total_ticks);

}
