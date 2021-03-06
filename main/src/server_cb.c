#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <glib.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include "include/server_cb.h"
#include "include/server.h"
#include "include/link_list.h"
#include "include/non_blocking_socket.h" /* -> event2/util.h + event2/listener.h */
#include "include/layout.h"
#include "include/utilities.h"
#include "include/score_db.h"
#include "include/question_db.h"

/* migrating to msg in json */
#include "include/json_msg.h"

/* work in progress */
#include "include/webserver_connector.h"

/* function table for server event callback */
GHashTable *func_table;

/* Print Ctrl character. eg. convert the \n into "0xA"... */
void buffrepr(const char * buff, int size, char * result, int resultSize)
{
	while (size && resultSize) {

		int print_count; 

		if(!isalpha(*buff))
			print_count = snprintf(result, resultSize, "0x%X", *buff);
		else
			print_count = snprintf(result, resultSize, "%c", *buff);

		resultSize -= print_count;
		result += print_count;
		--size;
		++buff;

		if (size && resultSize) {
			int print_count = snprintf(result, resultSize, " ");
			resultSize -= print_count;
			result += print_count;
		}
	}
}

/* control which question to display */
void display_question_cb(char x, char *value, struct bufferevent *bev)
{
	/* alter reactive cursor controling display of question */
	webserver_update_question(value);
	wprintw(msg_content, "Altering reactive cursor of display control of Web Server: %s\n", value);
	wrefresh(msg_content);
}

/* reveal answer */
void display_answer_cb(char x, char *value_int, struct bufferevent *bev)
{
	/* alter reactive display control cursor status to true */
	webserver_update_answer(1);
	wprintw(msg_content, "Requesting to show answer: \n");
	wrefresh(msg_content);
}

void buzzer_reset_cb(char x, char *value, struct bufferevent *bev)
{
	send_message(buzzerServer, buzzerPort, "0");
}

void buzzer_enable_cb(char x, char *value, struct bufferevent *bev)
{
	send_message(buzzerServer, buzzerPort, "1");
}

void buzzer_disable_cb(char x, char *value, struct bufferevent *bev)
{
	send_message(buzzerServer, buzzerPort, "w");
}

void ui_show_score_cb(char x, char *value, struct bufferevent *bev)
{
	send_message(webServer, webPort, "ui:{\"score\":\"show\"}");
}

void ui_hide_score_cb(char x, char *value, struct bufferevent *bev)
{
	send_message(webServer, webPort, "ui:{\"score\":\"hide\"}");
}

void retrieve_question_group_cb(char x, char *value, struct bufferevent *bev)
{
	char *json_str = retrieve_questions(value);
	bufferevent_write(bev, json_str, strlen(json_str));
}

/* initialize hash tables and store function pointers */
void hash_table_init()
{
	func_table = g_hash_table_new(g_str_hash, g_str_equal);

	/* score database operation */
	g_hash_table_insert(func_table, "Score+Answer", score_db_set);

	/* question answer operation */
	g_hash_table_insert(func_table, "Question+Next", display_question_cb);
	g_hash_table_insert(func_table, "Answer+", display_answer_cb);

	/* buzzer operation */
	g_hash_table_insert(func_table, "Buzzer+0", buzzer_reset_cb);
	g_hash_table_insert(func_table, "Buzzer+1", buzzer_enable_cb);
	g_hash_table_insert(func_table, "Buzzer+2", buzzer_disable_cb);

	/* UI operation */
	g_hash_table_insert(func_table, "UI+Show", ui_show_score_cb);
	g_hash_table_insert(func_table, "UI+Hide", ui_hide_score_cb);

	/* retrieve question groups */
	g_hash_table_insert(func_table, "Question+Retrieve", retrieve_question_group_cb);
}

/* server main loop and call back function to parse instruction from telnet */
void on_read_cb(struct bufferevent *bev, void *ctx)
{
	/*
	 * read from socket buffer
	 * evbuffer created to get input from buffer, obtain lenth of data in buffer with evbuffer_get_length()
	 * allot memory and call evbuffer_remove() to copy data into recvBuffer and drain data inside buffer
	 */
	char *recvBuff = NULL;
	struct evbuffer *input = bufferevent_get_input(bev);	/* read the buffer */
	struct Info *inf = ctx;					/* get information about the connection */
	size_t len = evbuffer_get_length(input);		/* get length */
	if(len) {
		wprintw(msg_content,"\nData of length %zu received from %s:%d\n", len, get_address_string(inf), get_port_int(inf));
		wrefresh(msg_content);
		recvBuff = (char*)malloc(sizeof(char)*(len+1));

		if(evbuffer_remove(input, recvBuff, len) < 0) {
			recvBuff[len] = 0;
			wprintw(msg_content, "DEBUG: on_read_cb(): copy form evbuffer failed!\n");
			wrefresh(msg_content);
		} else {
			recvBuff[len] = 0;
		}

	}

	/* DEBUG */
	char printBuffer[5000];
	buffrepr(recvBuff, len, printBuffer, sizeof(printBuffer));

	wprintw(msg_content, "Command Received: %s\n", printBuffer);
	wrefresh(msg_content);

	/* instruction=command catag, option=action to be taken; value=a char value; data=an int value */
	char *instruction, *option, *value, *data;

	/* Check if received JSON string is valid, proceed if so */
	if(decode_json(recvBuff, &instruction, &option, &data, &value)!=0) {
		char *str = encode_json("", "", "", "", 1);
		listBroadcast(theList, str);
		free(str);
	}
	else {
		/* parsing variables */
		/* a buffer for storing key to the hash */
		char buffer[5000];

		/* perform query on hash table */
		sprintf(buffer, "%s+%s", instruction, option);
		wprintw(msg_content, "search: %s\n", buffer);
		wrefresh(msg_content);

		/* begin lookup */
		gpointer *found;
		if((found=g_hash_table_lookup(func_table, buffer)) != NULL) {
			/* if result is found, perform requested function, else return error */
			void (*func)(char, char*, struct bufferevent*) = (void*)found;
			func(data[0], value, bev);

			/* send latest score to web server */
			score_publish();

			/* broadcast feedback and ack, free returned json string after use */
			char *str = encode_json(instruction, option, data, value, 1);
			wprintw(msg_content, "%s", str);
			wrefresh(msg_content);
			listBroadcast(theList, str);
			free(str);
		} else {
			/* show that request is invalid and broadcast ack */
			char *str = encode_json(instruction, option, data, value, 0);
			wprintw(msg_content, "request %s not found in hash table!\n", buffer);
			wprintw(msg_content, "%s", str);
			wrefresh(msg_content);
			listBroadcast(theList, str);
			free(str);
		}

		/* clear information */
		free(instruction); free(option); free(data); free(value);
	}

	/* free recvBuff after use */
	free(recvBuff);
}
