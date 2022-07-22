#include<stdio.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<poll.h>

void DieWithError(char *errorMessage);

// pollfdの中身
// int   fd;         /* file descriptor */
// short events;     /* requested events */
// short revents;    /* returned events */

//POLLIN
//呼び出し可能なデータがある

//poll(pollfd *fds, nfds_t nfds, int timeout_ms)
//ファイルディスクリプタのいずれか一つがI/Oを実行可能な状態になるのを待つ

int main(){
	int sockfd;				//通信用のソケット
	struct pollfd fds[2];	//poll()で見るための読み込みソケット
	int chkerr;		//chkerr:システムコールの返り値を保持する変数
	char buff[128];			//送受信用のデータ
	struct sockaddr_in serv_addr;	//サーバデータ

	printf("IPアドレスを入力:");
	scanf("%s",buff);

	//ソケットを作る
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) DieWithError("socket_error");

	//アドレスを作る
	memset(&serv_addr, 0, sizeof(struct sockaddr_in));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(buff);
	serv_addr.sin_port = htons(10000);

	printf("ユーザ名を入力してください: ");
	scanf("%s",buff);

	printf("接続中...\n");
	//コネクションを張るための要求をサーバに送る
	chkerr = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in));
	if(chkerr < 0) DieWithError("connect_error");

	fds[0].fd = sockfd;			//poll()で見る読み込みソケットとしてsockfdを登録
	fds[0].events = POLLIN;		//reventsにPOLLINが返ってくるように設定する

	fds[1].fd = 0;				//poll()で見る読み込みソケットとして標準入力を登録
	fds[1].events = POLLIN;		//reventsにPOLLINが返ってくるように設定する

	chkerr = write(fds[0].fd, buff, 128);	//データ(名前)の送信
	if(chkerr < 0) DieWithError("write_name_error");

	while(1){
		poll(fds, 2, 0);	//sockfdと標準入力を監視(fds[0]とfds[1])

		if(fds[1].revents == POLLIN){	//キーボードからの入力がある場合に入る
			fgets(buff, 128, stdin);
			chkerr = write(fds[0].fd, buff, 128);	//データの送信
			if(chkerr < 0) DieWithError("write_string_error");
		}

		else if(fds[0].revents == POLLIN){	//サーバからの通信要求がある場合に入る
			chkerr = read(fds[0].fd, buff, 128);	//データの受信
			if(chkerr < 0){
				DieWithError("read_error");
			}else if(chkerr == 0){
				//chkerrの値が0の時はサーバが"SERVER_END"以外で終了した場合なので、プログラムを終了する
				printf("＊＊サーバの予期せぬ終了＊＊\n");
				break;
			}

			if(strcmp(buff, "LOGOUT\n") == 0){
				//ログアウトの処理が行われた場合入り、プログラムを終了する
				printf("logoutします\n");
				break;
			}else if(strcmp(buff, "SERVER_END") == 0){
				//"SERVER_END"はサーバが終了する合図なので、プログラムを終了する
				printf("サーバが終了しました\n");
				break;
			}else{
				//それ以外のコメントが送られてきた場合
				printf("%s",buff);
			}
		}
	}

	//ソケットの終了
	chkerr = close(sockfd);
	if(chkerr < 0) DieWithError("close_sockfd_error");
}