#include <stdio.h>
#include <stdlib.h>
#include <my_global.h>
#include <mysql.h>
#include <string.h>
#include <stdbool.h>

typedef struct stucture
{
	//string question, answer, oa, ob, oc, od;
	char question[50];
        char answer[200];
        char option[4][50];
	char path[30];
}data;

void init_item(data* a, int track[40])
{
	strcpy(a->question, "\n");
        strcpy(a->answer, "\n");
        
	int index;
	for (index = 0; index < 4; index++);
	{
		strcpy(a->option[index], "\n");
	}
	
	strcpy(a->path, "\n");

	index = 0;
	while(index < 40)
	{
		track[index] = 0;
		index++;
	}
}

void update_data(data* a, MYSQL_ROW row)
{
	strcpy(a->question, (char*)row[1]);
	strcpy(a->answer, (char*)row[3]);
	if (strcmp(row[2], "MC") == 0)
	{
		int index;
		for (index = 0; index < 4; index++)
		{
			strcpy(a->option[index], (char*)row[index+4]);
		}
	}
	if (strcmp(row[8], "Y") == 0)
	strcpy(a->path, (char*)row[9]);
}

bool is_track(int track[40], int item)
{
	int index = 0;
	while(index < 40)
	{
		if(track[index] == 0) break;
		if(track[index] == item) return true;
		index++;
	}
	return false;
}

void update_track(int track[40], int item)
{
	int index = 0;
	while(index < 40)
	{
		if(track[index] == item)
		{
			printf("%s\n", "The item is already in the track");
			break;
		}
		if(track[index] == 0)
		{
			printf("%s\n", "The track has been updated");
			track[index] = item;
			return; 
		}
		index++;
	}
	printf("%s\n", "The track is already full");
}

int get_id()
{
	int id;
	while(true)
	{
		id = 1;
		if(id <= 40)
		{
			printf("%s\n", "The id has been retrieved successfully");
			break;
		}
		printf("%i is an unvalid id for query", id);
		printf("%s\n", "Please wait for another valid id");		
	}
	return id;
}

void init_con(MYSQL *con)
{
	if (con == NULL)
	{
		printf("%s", "The connection failed, Please try again.\n");
		exit(-1);
	}
	
	//(localhost, name of user, password, database)
	if (mysql_real_connect(con, "localhost", "testing", "testing", "interhouse", 0, NULL, 0) == NULL)
	{
		printf("%s", "The connection failed, Please enter the correct password again\n");
		mysql_close(con);
		exit(-1);
	}
}

MYSQL_RES* get_result(MYSQL* con, char* query)
{
	MYSQL_RES* result;
	printf("Query: %s\n\n", query);
	mysql_query(con, query);
	
	result = mysql_store_result(con);
	if (!result)
	{
		printf("%s\n", "Result");
		exit(-1);
	}
	return result;
}

int main()
{
	data* a;	
	int id;
	MYSQL_RES *result;
	MYSQL_ROW row;
	char query[100] = "Select * from data where id = ";
	char tid[3];
	int track[40];

	MYSQL *con = mysql_init(NULL);
	init_con(con);
	printf("%s", "The connection succeeded\n");	

	//The program starts with successful connection
	//items retreiving
	id = get_id();
	
	sprintf(tid, "%d", id);//convert id to cstring
	strcat (query, tid);//concatenation of the id and the query

	result = get_result(con, query);

	while((row = mysql_fetch_row(result)))
	{
		update_data(a, row);
	}
	//initialization above
	
	//testing and printing
	printf("Question: %s\n", a->question);
	printf("The answer is : %s\n", a->answer);
	printf("A: %s\n", a->option[0]);
	printf("B: %s\n", a->option[1]);
	printf("C: %s\n", a->option[2]);
	printf("D: %s\n", a->option[3]);

	mysql_free_result(result);
	mysql_close(con);

	return 0;
}