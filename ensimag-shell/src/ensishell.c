/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "variante.h"
#include "readcmd.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#ifndef VARIANTE 
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */
struct job {
	pid_t pid;
	char* nom_cmd;
	struct timeval temps;
	struct job* next;
};
struct job *JOBS = NULL;

void ajout(pid_t pid, char* nom) {
	struct job * nouv = malloc(sizeof(struct job));
	nouv -> pid = pid;
	char* cpy_nom = malloc(sizeof(char) * strlen(nom));
	strcpy(cpy_nom, nom);
	nouv -> nom_cmd = cpy_nom;
	gettimeofday(&(nouv-> temps), NULL);
	nouv -> next = NULL;
	if (JOBS == NULL) {
		JOBS = nouv;
	} else {
		struct job * current = JOBS;
		while (current -> next != NULL) {
			current = current -> next;
		}
		current -> next = nouv;
	}
}

void supprime(pid_t pid) {
	struct job * ancient = JOBS;
	struct job * current = JOBS -> next;
	if (ancient -> pid == pid) {
		JOBS = current;
	} else {
		while (current != NULL && current -> pid != pid) {
			current = current -> next;
		}
		if (current != NULL) {
			ancient -> next = current -> next;
		}
	}
}

void mis_a_jour(void) {
	struct job * current = JOBS;
	if (JOBS != NULL) {
		while (current != NULL) {
			if (waitpid(current -> pid, NULL, WNOHANG)) {
				supprime(current -> pid);
			}
			current = current -> next;
		}
	}
}

void print_jobs(void) {
	mis_a_jour();
	struct job * current = JOBS;
	if (JOBS != NULL) {
		while (current != NULL) {
			printf("PID: %i, CMD: %s \n", current -> pid, current -> nom_cmd);
			current = current -> next;
		}
	} else {
		printf("Aucune tâche en arrière-plan\n");
	}
}

static void dead(int sig, siginfo_t *siginfo, void *context) {
	struct job * current = JOBS;
	if (JOBS != NULL) {
		while (current != NULL) {
			if (current -> pid == siginfo -> si_pid) {
				struct timeval fin;
				gettimeofday(&fin, NULL);
				int temps = (double) fin.tv_sec - (double)(current -> temps).tv_sec;
				printf("PID : %i is dead. It lasted %d s\n", current -> pid, temps);
			}
			current = current -> next;
		}
	}
}

void command(char** cmd, struct cmdline * l) {
	if (strcmp(cmd[0],"jobs") == 0) {
		print_jobs();
	} else {
		pid_t pid = fork();
		if (pid == 0) {
			char reponse[60];
			char * chemin = "/bin/";
			char * vide = "";
			if (cmd[0][0] != '/') {
				strcpy(reponse, chemin);
			} else {
				strcpy(reponse, vide);
			}
			strcat(reponse, cmd[0]);
			if (l->bg) {
				setpgid(0,0);
			}
			if (l -> in) {
				int file_in = open(l->in, O_RDONLY, 0);
				dup2(file_in, STDIN_FILENO);
				close(file_in);
			}
			if (l -> out) {
				int file_out = creat(l->out, S_IRWXU);
				dup2(file_out, STDOUT_FILENO);
				close(file_out);
			}
			execvp(cmd[0], cmd);
			printf("bash : %s : command not found ...\n", cmd[0]);
		} else {
			if (! l->bg) {
				waitpid(pid, NULL, 0);
			} else {
				ajout(pid, cmd[0]);
				struct sigaction act;
				memset(&act, '\0', sizeof(act));
				act.sa_sigaction = &dead;
				act.sa_flags = SA_SIGINFO;
				if (sigaction(SIGCHLD, &act, NULL) < 0) {
					perror("sigaction");
				}
			}
		}
	}
}

void tube(char ** cmd1, char ** cmd2, struct cmdline *l) {
	if (strcmp(cmd1[0],"jobs") == 0) {
		command(cmd2, l);
	} else if (strcmp(cmd2[0],"jobs") == 0) {
		print_jobs();
	} else {
		pid_t pid = fork();
		if (pid == 0) {
			int fd[2];
			pipe(fd);
			pid_t child_pid = fork();
			if (child_pid == 0) {
				dup2(fd[1], STDOUT_FILENO);
				close(fd[0]);
				close(fd[1]);
				if (l -> in) {
					int file_in = open(l->in, O_RDONLY, 0);
					dup2(file_in, STDIN_FILENO);
					close(file_in);
				}
				execvp(cmd1[0], cmd1);
				printf("bash : %s : command not found ...\n", cmd1[0]);
			} else {
				dup2(fd[0], STDIN_FILENO);
				close(fd[0]);
				close(fd[1]);
				if (l -> out) {
					int file_out = creat(l->out, S_IRWXU);
					dup2(file_out, STDOUT_FILENO);
					close(file_out);
				}
				execvp(cmd2[0], cmd2);
				printf("bash : %s : command not found ...\n", cmd2[0]);
			}
		} else {
				waitpid(pid, NULL, 0);
			}
	}
}

void loop_tube(struct cmdline* l) {
	int p[2];
	pid_t pid;
	int file_in = 0;
	char ** cmd = l -> seq[0];
	int i = 0;
	while (cmd != 0) {
		pipe(p);
		if ((pid = fork()) == -1) {
			exit(EXIT_FAILURE);
		} else if (pid == 0) {
			if (l-> seq[i+1] != NULL) {
				dup2(p[1], STDOUT_FILENO);
			} else {
				if (l -> out) {
					int file_out = creat(l->out, S_IRWXU);
					dup2(file_out, STDOUT_FILENO);
					close(file_out);
				}
			}
			close(p[0]);
			if (l -> in && i == 0) {
				int file_in = open(l->in, O_RDONLY, 0);
				dup2(file_in, STDIN_FILENO);
				close(file_in);
			} else {
				dup2(file_in, 0);
			}
			execvp(cmd[0], cmd);
			printf("bash : %s : command not found ...\n", cmd[0]);
		} else {
			wait(NULL);
			close(p[1]);
			file_in = p[0];
			i++;
			cmd = l -> seq[i];
		}
	}
}

#if USE_GUILE == 1
#include <libguile.h>

int question6_executer(char *line)
{
	/* Question 6: Insert your code to execute the command line
	 * identically to the standard execution scheme:
	 * parsecmd, then fork+execvp, for a single command.
	 * pipe and i/o redirection are not required.
	 */
	struct cmdline *l = parsecmd( & line);
	command(l->seq[0], l);
	return 0;
}

SCM executer_wrapper(SCM x)
{
        return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif


void terminate(char *line) {
#if USE_GNU_READLINE == 1
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
	  free(line);
	printf("exit\n");
	exit(0);
}


int main() {
        printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#if USE_GUILE == 1
        scm_init_guile();
        /* register "executer" function in scheme */
        scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

	while (1) {
		struct cmdline *l;
		char *line=0;
		// int i, j;
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		   can not be cleaned at the end of the program. Thus
		   one memory leak per command seems unavoidable yet */
		line = readline(prompt);
		if (line == 0 || ! strncmp(line,"exit", 4)) {
			terminate(line);
		}

#if USE_GNU_READLINE == 1
		add_history(line);
#endif


#if USE_GUILE == 1
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
                        continue;
                }
#endif

		/* parsecmd free line and set it up to 0 */
		l = parsecmd( & line);

		/* If input stream closed, normal termination */
		if (!l) {
			terminate(0);
		}

		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		if (l->in) printf("in: %s\n", l->in);
		if (l->out) printf("out: %s\n", l->out);
		if (l->bg) printf("background (&)\n");

		/* Display each command of the pipe */
		// for (i=0; l->seq[i]!=0; i++) {
		// 	char **cmd = l->seq[i];
		// 	printf("seq[%d]: ", i);
    //                     for (j=0; cmd[j]!=0; j++) {
    //                             printf("'%s' ", cmd[j]);
    //                     }
		// 	printf("\n");
		// }

		if (l->seq[1] == 0) {
			command(l->seq[0], l);
		} else if (l-> seq[2] == 0) {
			tube(l->seq[0], l-> seq[1], l);
		} else {
			loop_tube(l);
		}
	}

}
