#include<stdio.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<poll.h>

#define MAX_USER 10 		//接続するユーザの上限

void DieWithError(char *errorMessage);

//ユーザ(クライアント)の情報を持つ構造体
struct user{
	char name[128];		//クライアントの名前
	int login;			//1でログイン状態,0でログアウト状態
	struct pollfd new_sockfd[1];	//pollで見る読み込みソケット
};


int main(){
	int sockfd;			//受付用ソケット
	struct user user[MAX_USER];		//クライアントを管理する配列
	char buff[128], new_buff[128];	//送受信データ
	struct pollfd fds[2];			//poll()で見るための読み込みソケット
	struct sockaddr_in serv_addr;	//サーバデータ
	int chkerr, i, j, k, num_user = 0;	//chkerr：システムコールの返り値を保持する変数
				//i,j,k：カウンタ変数 //nuser：ログイン状態のクライアントの人数

	//全クライアント用のログイン状態を初期化
	for(i=0;i<MAX_USER;i++){
		user[i].login = 0;
	}

	//ソケットを作る
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) DieWithError("socet_error");	//エラーチェック,エラー時myerrorを呼ぶ

	//アドレスを作る
	memset(&serv_addr, 0, sizeof(struct sockaddr_in));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(10000);

	//ソケットにアドレスを割り当てる
	chkerr = bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in));
	if(chkerr < 0) DieWithError("bind_error");

	fds[0].fd = sockfd;			//poll()で見る読み込みソケットとしてsockfdを登録
	fds[0].events = POLLIN;		//reventsにPOLLINが返ってくるように設定する

	fds[1].fd = 0;				//poll()で見る読み込みソケットとして標準入力を登録
	fds[1].events = POLLIN;		//reventsにPOLLINが返ってくるように設定する

	chkerr = listen(fds[0].fd, 5); //コネクション要求を待ち始めるよう指示
	if(chkerr < 0) DieWithError("listen_error");

	printf("このサーバーの最大接続人数は%dです\n",MAX_USER);
	while(1){
		poll(fds,2,0);	//sockfdと標準入力を監視(fds[0]とfds[1])

		if(fds[0].revents == POLLIN  &&  num_user < MAX_USER){
		//sockfdに通信要求がある場合入る。クライアントの人数が上限に達している場合は入らない
			for(j=0; j< MAX_USER; j++){
				if(user[j].login != 1){	//ログインされていないuser[j]にクライアントを登録
					user[j].new_sockfd[0].fd = accept(fds[0].fd, NULL, NULL);	//fdを配列に保存
					if(user[j].new_sockfd[0].fd == -1) DieWithError("accept_error");

					user[j].new_sockfd[0].events = POLLIN;		//reventsにPOLLINが返ってくるように設定する

					chkerr = read(user[j].new_sockfd[0].fd, buff, 128);	//クライアントからデータ(クライアントの名前)を受け取る
					if(chkerr < 0) DieWithError("read_name_error");

					//同じ名前のクライアントを受け付けない
					for(k=0; k<MAX_USER; k++){
						if(user[k].login){
							if(strcmp(user[k].name, buff) == 0){
								chkerr = write(user[j].new_sockfd[0].fd, "SERVER_END", 128);
								if(chkerr < 0) DieWithError("same_name_error");
							}
						}
					}

					for(k=0; k<MAX_USER; k++){
						if(strcmp(user[k].name, buff) == 0){
								DieWithError("That name is used");
							}
					}

					strcpy(user[j].name, buff);	//nameに名前を保存する
					user[j].login = 1;			//ログイン状態にする
					num_user++;				//クライアントの人数を1人増やす
					sprintf(new_buff,"->%sさんがloginしました ＊残り接続可能数:%d\n",buff, MAX_USER-num_user);
					if(chkerr < 0) DieWithError("sprintf_error");

					if(num_user == MAX_USER){	//最大人数に達した時警告文をいれる
						sprintf(buff, "%s＊＊ユーザ人数が上限になりました＊＊\n", new_buff);
						if(chkerr < 0) DieWithError("sprintf_error");
						strcpy(new_buff, buff);
					}

					for(i=0;i<MAX_USER;i++){	//全クライアントに送信する
						if(user[i].login){
							chkerr = write(user[i].new_sockfd[0].fd, new_buff, 128);
							if(chkerr < 0) DieWithError("write_comment_error");
							if(i != j){

								chkerr = sprintf(buff,"->%sさんがloginしています\n",user[i].name);
								if(chkerr < 0) DieWithError("sprintf_error");

								chkerr = write(user[j].new_sockfd[0].fd, buff, 128);
								if(chkerr < 0) DieWithError("write_comment_error");
							}
						}
					}
					printf("%s",new_buff);
					break;
				}
			}
		}

		for(j=0;j<MAX_USER;j++){
			poll(user[j].new_sockfd,1,0);

			if(user[j].new_sockfd[0].revents == POLLIN  &&  user[j].login == 1){
			//クライアントからの通信要求がある場合に入る。ログイン状態でない場合は入らない

				chkerr = read(user[j].new_sockfd[0].fd, buff, 128);	//データの受け取り

				if(chkerr < 0){
					DieWithError("read_comment_error");
				}else if(chkerr == 0){
				//chkerrが0の場合、"LOGOUT"入力以外の方法でクライアントとのソケットが切れた場合入る
					strcpy(buff, "LOGOUT\n");		//LOGOUT扱いにする
				}

				if(strcmp(buff, "LOGOUT\n") == 0){	//"LOGOUT"が入力された時
					chkerr = write(user[j].new_sockfd[0].fd, buff, 128);	//クライアントに"LOGOUT"を送りソケットをcloseさせる
					if(chkerr < 0) DieWithError("write_error");

					num_user--;	//クライアントを1人減らす
					user[j].login = 0;	//ログアウト状態にする

					chkerr = sprintf(new_buff, "<-%sさんがlogoutしました ＊残り接続可能数:%d\n", user[j].name, MAX_USER-num_user);
					if(chkerr < 0) DieWithError("sprintf_error");
				}
				
				else{	//それ以外のコメントが送られた場合
					chkerr = sprintf(new_buff, "%s: %s\n", user[j].name, buff);
					if(chkerr < 0) DieWithError("sprintf_error");
				}

 				printf("%s",new_buff);

				for(i=0;i<MAX_USER;i++){	//全クライアントに送信する
					if(user[i].login){
						chkerr = write(user[i].new_sockfd[0].fd, new_buff, 128);
        				if(chkerr < 0) DieWithError("write_comment_error");
					}
				}
			}
		}
		if(fds[1].revents == POLLIN){	//キーボードからの入力がある場合に入る
			scanf("%s",buff);
			if(strcmp(buff, "SERVER_END") == 0){	//"SERVER_END"から入力された場合while文から抜ける
				break;
			}
			else{
				printf("無効なコマンドです\n");
			}
		}
	}
	for(i=0;i<MAX_USER;i++){	//全クライアントにソケットをcloseするよう送信する
		if(user[i].login){
			chkerr = write(user[i].new_sockfd[0].fd, "SERVER_END", 128);
			if(chkerr < 0) DieWithError("write_serverend_error");
		}
	}


	//全クライアントのソケットcloseするまで待つ
	for(i=0;i<MAX_USER;i++){
		if(user[i].login){
			while(1){
				chkerr = read(user[i].new_sockfd[0].fd, buff, 128);
				if(chkerr < 0){
					DieWithError("close_read_error");
				}else if(chkerr == 0){
					//chkerrの値が0ならばソケットがcloseされている
					break;
				}
			}
		}
	}

	printf("サーバーを終了します\n");

	//ソケットを終了する
	chkerr = close(sockfd);
	if(chkerr < 0) DieWithError("close_sockfd");
}