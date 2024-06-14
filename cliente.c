#include "sp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static	char	User[80];
static  char    Spread_name[80];
static  char    Private_group[MAX_GROUP_NAME];
static  mailbox Mbox;
char menu_text[1024 * 1024];

void print_menu();
void load_menu_text(char* file_name);
void *read_messages(); //Para el thread
void join_group(char *group);
void send_message(char *group);
void leave_group(char *group);

int main(int argc, char *argv[]){
	load_menu_text("./menu.txt");
	
	int	ret;
	/* connecting to the daemon, requesting group information */
	/*
	Spread_Name: 4803@localhost (por defecto)
	User: usa el nombre del contenedor
	*/
	ret = SP_connect( Spread_name, User, 0, 1, &Mbox, Private_group );
	if( ret < 0 ) 
	{
		SP_error( ret );
		exit(0);
	}
	
	printf("Se estableció conexión con el servidor de Spread.\n");
	
	// Crear hilo para recibir mensajes
	pthread_t reader;
	if (pthread_create(&reader, NULL, read_messages, NULL) != 0) {
        perror("Error al crear el hilo");
        return 1;
    }
	
	char *cmd = malloc(1024 * sizeof(char));
	
	print_menu();	
	do{
		fgets(cmd, 1024, stdin);
		size_t len = strlen(cmd);
		if (len > 1 && cmd[len - 1] == '\n') {
			cmd[len - 1] = '\0';
		}
		
		if(!strcmp(cmd, "o"))
			print_menu();
		else {
			char *token = strtok(cmd, " "); // Obtengo el parametro de la operación
			if(!strcmp(token, "j")){
				// JOIN GROUP
				token = strtok(NULL, " ");
				join_group(token);
			}else if(!strcmp(token, "l")){
				// LEAVE GROUP
				token = strtok(NULL, " ");
				leave_group(token);
			}else if(!strcmp(token, "s")){
				// SEND MESSAGE
				token = strtok(NULL, " ");
				send_message(token);
			}else if(strcmp(token, "q")){
				// NO CORRESPONDE A NINGUNA OPCION
				printf("No se ha ingresado una opción correcta, utilize 'o' para ver las opciones.\n");
			}
		}
		
	}while(strcmp(cmd, "q") != 0);
	
	free(cmd);
	SP_disconnect (Mbox);
	return 0;
}

void load_menu_text(char* file_name){
	FILE *file = fopen(file_name, "r");
    if (file == NULL) {
        perror("Error al abrir el archivo");
        exit(EXIT_FAILURE);
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        strcat(menu_text, buffer);
    }

    if (ferror(file)) {
        perror("Error al leer el archivo");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    fclose(file);
}

void print_menu(){	
	printf("\n%s\n", menu_text);
}

void join_group(char *group){
	SP_join(Mbox, group);
}

void *read_messages(){
	// Mensajes de membresia de grupo
	static	char		mess[102400];
	char		sender[MAX_GROUP_NAME];
	char		target_groups[100][MAX_GROUP_NAME];
	int		num_groups;
    membership_info memb_info;
	int		service_type;
	int16		mess_type;
	int		endian_mismatch;
	int		i;
	int		ret;
	
	
    service_type = 0;
	do{
		ret = SP_receive( Mbox, &service_type, sender, 100, &num_groups, target_groups, 
			&mess_type, &endian_mismatch, sizeof(mess), mess );
		
		if( ret < 0 ) 
		{
			SP_error( ret );
			exit(0);
		}
		
		printf("\n============================\n");
		
		if( Is_regular_mess( service_type ) )
		{
			/* A regular message, sent by one of the processes */
			mess[ret] = 0;
			if     ( Is_unreliable_mess( service_type ) ) printf("received UNRELIABLE ");
			else if( Is_reliable_mess(   service_type ) ) printf("received RELIABLE ");
			else if( Is_fifo_mess(       service_type ) ) printf("received FIFO ");
			else if( Is_causal_mess(     service_type ) ) printf("received CAUSAL ");
			else if( Is_agreed_mess(     service_type ) ) printf("received AGREED ");
			else if( Is_safe_mess(       service_type ) ) printf("received SAFE ");
			printf("message from %s of type %d (endian %d), to %d groups \n(%d bytes): %s\n",
				sender, mess_type, endian_mismatch, num_groups, ret, mess );

		}else if ( Is_reg_memb_mess( service_type ) ){
			printf("Received REGULAR membership for group %s with %d members, where i am member %d:\n", sender, num_groups, mess_type);
			for( i=0; i < num_groups; i++ )
				printf("\t%s\n", &target_groups[i][0] );
			printf("grp id is %d %d %d\n",memb_info.gid.id[0], memb_info.gid.id[1], memb_info.gid.id[2] );
			if( Is_caused_join_mess( service_type ) ) printf("Due to the JOIN.\n");
			if( Is_caused_leave_mess( service_type ) ) printf("caused by LEAVE.\n");
			if( Is_caused_disconnect_mess( service_type ) ) printf("caused by DISCONNECT.\n");
		}
	}while(1);
	
}

void send_message(char *group){
	char buffer[1024];
	printf("Enter a message: ");
	fgets(buffer, 1024, stdin);
	
	printf("Tipo de mensaje a enviar:\n\t1. UNRELIABLE_MESS \n\t2. RELIABLE_MESS \n\t3. FIFO_MESS \n\t4. CAUSAL_MESS \n\t5. AGREED_MESS  \n\t6. SAFE_MESS\n");
	int opt;
	scanf("%d", &opt);
	service service_type;
	switch(opt){
		case 1:
			service_type = UNRELIABLE_MESS;
			break;
		case 2:
			service_type = RELIABLE_MESS;
			break;
		case 3:
			service_type = FIFO_MESS;
			break;
		case 4:
			service_type = CAUSAL_MESS;
			break;
		case 5:
			service_type = AGREED_MESS;
			break;
		case 6:
			service_type = SAFE_MESS;
			break;
		default:
			printf("No se eligió una opción correcta.\n");
			return;
	}
	
	SP_multicast(Mbox, service_type, group, 1, strlen(buffer), buffer);
	
}

void leave_group(char *group){
	int ret = SP_leave(Mbox, group);
	if(ret == 0)
		printf("Te has salido del grupo %s.\n", group);
}