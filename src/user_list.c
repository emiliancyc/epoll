#include "user_list.h"
#include <stdlib.h>
#include <string.h>

struct ul_ctx{
	size_t size;
	user** users;
};

static user* find_by_fd(user_list* ul, int fd);
static int rm_by_fd(user_list* ul, int fd);
static int add(user_list* ul, user* u);

user_list* create_ul(size_t size){
	user_list* res = malloc(sizeof(user_list));
	res->ctx = malloc(sizeof(ul_ctx));
	res->ctx->size = size;
	res->ctx->users = malloc(size* sizeof(user*));
	memset(res->ctx->users,0,(size* sizeof(user*)));
	//set methods pointers here

	res->add = add;
	res->find_by_fd = find_by_fd;
	res->rm_by_fd = rm_by_fd;

	return res;
}

void destroy_ul(user_list* ul){
	size_t i = 0;
	for (;i < ul->ctx->size;++i){
		if(ul->ctx->users[i]){
			free(ul->ctx->users[i]->nick);
			free(ul->ctx->users[i]);
			ul->ctx->users[i] = 0;
		}
	}
	free(ul->ctx);
	free(ul);
}

static user* find_by_fd(user_list* ul, int fd){
	size_t i = 0;
	for(;i < ul->ctx->size;++i){
		if(ul->ctx->users[i] && (ul->ctx->users[i]->fd == fd)){
			return ul->ctx->users[i];
		}
	}
	return 0; //return NULL if not found
}

static int add(user_list* ul, user* u){
	size_t i = 0;
	if(find_by_fd(ul, u->fd) != 0){
		return 1; //user already exist
	}
	for(;i < ul->ctx->size;++i){
		if(ul->ctx->users[i] == 0){
			ul->ctx->users[i] = u;
			return 0;
		}
	}
	return 1; //there is no place in user list
}

static int rm_by_fd(user_list* ul, int fd){
	size_t i = 0;
	for(;i < ul->ctx->size; ++i){
		if(ul->ctx->users[i] && (ul->ctx->users[i]->fd == fd)){
			free(ul->ctx->users[i]->nick);
			free(ul->ctx->users[i]);
			ul->ctx->users[i] = 0;
			return 0;
		}
	}
	return 1;//user not found
}

