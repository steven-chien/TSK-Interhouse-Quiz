#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <search.h>

#include "include/server_cb.h"
#include "include/server.h"
#include "include/link_list.h"
#include "include/non_blocking_socket.h" /* -> event2/util.h + event2/listener.h */
#include "include/layout.h"
#include "include/db_redis.h"
#include "include/utilities.h"
#include "include/score_db.h"

/* pending for removal */
#include "include/score.h"

struct Server_func_cb {
	char name[10];
	void (*func)(char, char*);
};

struct hsearch_data func_table;

void display_question_cb(char x, char *value) {

	char question_json[500];

	//to read a question from database
	wprintw(msg_content, "Reading Question: %s\n", value);
	
	//procedure to get json string from database module
	db_con *con = db_connect();									//initiate mysql connection
	sprintf(question_json, "question:%s\n", db_get_result(con, value));	//get question with question ID and store in buffer
	db_close(con);											//close the connection
	
	//send message to webserver to show question
	wprintw(msg_content, "Sending Question to Web Server: \n");
	wrefresh(msg_content);
	send_message(webServer, webPort, question_json);
}

void display_answer_cb(char x, char *value_int) {

	wprintw(msg_content, "Requesting to show answer: \n");
	wrefresh(msg_content);
	send_message(webServer, webPort, "answer:{}");
}

void buzzer_reset_cb(char x, char *value) {

	send_message(buzzerServer, buzzerPort, "0");
}

void buzzer_enable_cb(char x, char *value) {

	send_message(buzzerServer, buzzerPort, "1");
}

void buzzer_disable_cb(char x, char *value) {

	send_message(buzzerServer, buzzerPort, "w");
}

void ui_show_score_cb(char x, char *value) {

	send_message(webServer, webPort, "ui:{\"score\":\"show\"}");
}

void ui_hide_score_cb(char x, char *value) {

	send_message(webServer, webPort, "ui:{\"score\":\"hide\"}");
}

//initialize hash tables and store function pointers
void hash_table_init() {

	memset(&func_table, 0, sizeof(struct hsearch_data));
	hcreate_r(100, &func_table);

	ENTRY item;
	ENTRY *ret;

	/* old score calculation and storage mechanism, pending for removal */
	// score operation
	item.key = "Score+Minus";
	item.data = (void*)malloc(sizeof(struct Server_func_cb));
	((struct Server_func_cb*)(item.data))->func = &minus_score;
	hsearch_r(item, ENTER, &ret, &func_table);

	item.key = "Score+Add";
	item.data = (void*)malloc(sizeof(struct Server_func_cb));
	((struct Server_func_cb*)(item.data))->func = &add_score;
	hsearch_r(item, ENTER, &ret, &func_table);

	item.key = "Score+Update";
	item.data = (void*)malloc(sizeof(struct Server_func_cb));
	((struct Server_func_cb*)(item.data))->func = &update_score;
	hsearch_r(item, ENTER, &ret, &func_table);
	/* pending for removal */

	// score database operation
	item.key = "Score+Answer";
	item.data = (void*)malloc(sizeof(struct Server_func_cb));
	((struct Server_func_cb*)(item.data))->func = &score_db_set;
	hsearch_r(item, ENTER, &ret, &func_table);

	// question answer operation
	item.key = "Question+Next";
	item.data = (void*)malloc(sizeof(struct Server_func_cb));
	((struct Server_func_cb*)(item.data))->func = &display_question_cb;
	hsearch_r(item, ENTER, &ret, &func_table);

	item.key = "Answer+";
	item.data = (void*)malloc(sizeof(struct Server_func_cb));
	((struct Server_func_cb*)(item.data))->func = &display_answer_cb;
	hsearch_r(item, ENTER, &ret, &func_table);

	// buzzer operation
	item.key = "Buzzer+0";
	item.data = (void*)malloc(sizeof(struct Server_func_cb));
	((struct Server_func_cb*)(item.data))->func = &buzzer_reset_cb;
	hsearch_r(item, ENTER, &ret, &func_table);

	item.key = "Buzzer+1";
	item.data = (void*)malloc(sizeof(struct Server_func_cb));
	((struct Server_func_cb*)(item.data))->func = &buzzer_enable_cb;
	hsearch_r(item, ENTER, &ret, &func_table);

	item.key = "Buzzer+2";
	item.data = (void*)malloc(sizeof(struct Server_func_cb));
	((struct Server_func_cb*)(item.data))->func = &buzzer_enable_cb;
	hsearch_r(item, ENTER, &ret, &func_table);

	// UI operation
	item.key = "UI+Show";
	item.data = (void*)malloc(sizeof(struct Server_func_cb));
	((struct Server_func_cb*)(item.data))->func = &ui_show_score_cb;
	hsearch_r(item, ENTER, &ret, &func_table);

	item.key = "UI+Hide";
	item.data = (void*)malloc(sizeof(struct Server_func_cb));
	((struct Server_func_cb*)(item.data))->func = &ui_hide_score_cb;;
	hsearch_r(item, ENTER, &ret, &func_table);

}

//server main loop and call back function to parse instruction from telnet
void on_read_cb(struct bufferevent *bev, void *ctx)
{
	//read from socket buffer
	//evbuffer created to get input from buffer, obtain lenth of data in buffer with evbuffer_get_length()
	//allot memory and call evbuffer_remove() to copy data into recvBuffer and drain data inside buffer
	char *recvBuff;
	struct evbuffer *input = bufferevent_get_input(bev);	//read the buffer
	struct Info *inf = ctx;									//get information about the connection
	size_t len = evbuffer_get_length(input);				//get length
	if(len) {
		wprintw(msg_content,"Data of length %zu received from %s:%s\n", len, inf->address, inf->port);
		wrefresh(msg_content);
		recvBuff = (char*)malloc(sizeof(char)*(len+1));
		if(evbuffer_remove(input, recvBuff, len)<0) {
			recvBuff[len] = 0;
			wprintw(msg_content, "DEBUG: on_read_cb(): copy form evbuffer failed!\n");
			wrefresh(msg_content);
		}
		else {
			recvBuff[len] = 0;
		}
		
	}

	//parsing variables
	//instruction=command catag, option=action to be taken; value=a char value; data=an int value
	char instruction[10], option[10], value[100], data;	//for sscanf

	//a buffer for storing returned string from functions
	char buffer[5000];
	
	//DEBUG
	wprintw(msg_content, "Command Received: %s\n", recvBuff);
	wrefresh(msg_content);

	//process instruction
	sscanf(recvBuff, "%s %s %c %s", instruction, option, &data, value);

	//free recvBuff after use
	free(recvBuff);
	
	//perform query on hash table
	sprintf(buffer, "%s+%s", instruction, option);
	wprintw(msg_content, "serach: %s\n", buffer);
	wrefresh(msg_content);

	ENTRY entry;
	entry.key = buffer;
	ENTRY *found;

	if(hsearch_r(entry, FIND, &found, &func_table)!=0) {
		//found->data->func(data, value);
		void (*func)(char, char*) = ((struct Server_func_cb*)(found->data))->func;
		func(data, value);

		/* backward compatibility, pending for removal */
		//push score to webserver
		//save_score("score_backup.dat");
		push_score(webServer, webPort);
		/* pending for removal */

		// send latest score to web server
		score_publish();

		//broadcast feedback and ack
		listBroadcast(theList, "ACK from server");
	}
	else {
		wprintw(msg_content, "request %s not found in hash table!\n", entry.key);
		wrefresh(msg_content);
		listBroadcast(theList, "invalid command");
	}
	//clear information
	memset(&recvBuff, 0, sizeof(recvBuff));
	memset(&buffer, 0, sizeof(buffer));
}