#include <iostream>
#include <fstream>
#include <string>
#include <dirent.h>
#include <sstream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>  
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <algorithm>
#include <openssl/md5.h>
#include <sys/stat.h>
#include <iomanip>   //for setfill
#include <algorithm> //for transfrom

using namespace std;

#define BUFFSIZE 16384 
string get_md5hash( const string& fname) { 
   char buffer[BUFFSIZE]; 
   unsigned char digest[MD5_DIGEST_LENGTH]; 
   stringstream ss; 
   string md5string; 
   ifstream ifs(fname, ifstream::binary); 
   MD5_CTX md5Context; 
   MD5_Init(&md5Context);  
   while (ifs.good()) { 
      ifs.read(buffer, BUFFSIZE); 
      MD5_Update(&md5Context, buffer, ifs.gcount());  
   }  
   ifs.close();  
   int res = MD5_Final(digest, &md5Context);     
   if( res == 0 ) // hash failed 
      return {};   // or raise an exception 
   // set up stringstream format 
   ss << hex << uppercase << setfill('0');  
   for(unsigned char uc: digest) {ss << setw(2) << (int)uc;}  
   md5string = ss.str();
   transform(md5string.begin(), md5string.end(), md5string.begin(), ::tolower);   
   return md5string; 
}

bool compare_strings(string a, string b) {
   string c=a, d=b;
   for(int x=0; x<c.size(); x++) {
      c[x] = ::toupper(c[x]);
   }
   for(int x=0; x<d.size(); x++) {
      d[x] = ::toupper(d[x]);
   }
   if (c.compare(d) <= 0) return true;
   else {return false;}
   // now x is the index where mismatch is there
}

void list_directory(const char *dir_path, vector<string> &files_with_me) {
   struct dirent *file;
   DIR *dir = opendir(dir_path);
   if (dir == NULL) {
      return;
   }

   while ((file = readdir(dir)) != NULL) {
        string f_name = string(file->d_name);
        if (!(f_name.compare(".") == 0 || f_name.compare("..") == 0 || f_name.compare("Downloaded") == 0)) {
            files_with_me.push_back(f_name);
        }
   }
   closedir(dir);
   sort(files_with_me.begin(), files_with_me.end(), compare_strings);
   for(int i=0; i<files_with_me.size(); i++) {cout << files_with_me[i] << endl;}
   
}

void client_side(bool &allMyClients,int c_port,int c_id,int c_unique_id,vector<int> nb_num,vector<int> nb_port,int num_nb, vector<string> files_for_search, vector<string> files_i_hv, string dir_path) {
   if (num_nb < 1) {
      for(int i=0; i<files_for_search.size(); i++) {
         cout << "Found "+files_for_search[i]+" at 0 with MD5 0 at depth 0" << endl;
      }
      return;
   }
   vector<int> client_socket, nb_uniqId(num_nb);
   int num_connected=0;
   for(int i=0;i<num_nb;i++) {
      client_socket.push_back(socket(PF_INET,SOCK_STREAM,0));
   }
   vector<sockaddr_in> server_nb_addr;
   bool cnctd[num_nb];
   for(int i=0;i<num_nb;i++)  cnctd[i] = 0;

   string str_file_list = to_string(c_port)+" "+to_string(c_unique_id)+" "+to_string(files_i_hv.size());
   for(int i=0; i<files_i_hv.size(); i++) {
      str_file_list += " "+files_i_hv[i];
   }

   char buff1[100];   //server response
   for(int x=0; x<100; x++) buff1[x] = 0;

   while(num_connected < num_nb) { 
      for(int i=0;i<num_nb;i++) {
         struct sockaddr_in serveraddr;
         memset(&serveraddr,0,sizeof(serveraddr));
         serveraddr.sin_family = AF_INET;
         serveraddr.sin_port = htons(nb_port[i]);
         serveraddr.sin_addr.s_addr = INADDR_ANY;

         server_nb_addr.push_back(serveraddr);

         if(cnctd[i]==0) {         
            int connec_sts = connect(client_socket[i],(struct sockaddr *) &(server_nb_addr[i]),sizeof(server_nb_addr[i]));
            if(connec_sts == -1) continue;
            else {
               cnctd[i] =1;
               num_connected++;
               for(int x=0; x<100; x++) buff1[x] = 0;
               send(client_socket[i],str_file_list.c_str(),strlen(str_file_list.c_str()),0);
               recv(client_socket[i],buff1,100,0);
               nb_uniqId[i] = atoi(buff1);
               
            }        
         }       
      }
   }

   for(int i=0;i<num_nb;i++) {
      cout << "Connected to " + to_string(nb_num[i]) + " with unique-ID " + to_string(nb_uniqId[i]) + " on port " + to_string(nb_port[i]) << endl;
   } 

   bool directory_made = false;
   int curr_id =0, sock_ind = -1;
   for(int i=0; i<files_for_search.size(); i++) {
      curr_id =0, sock_ind = -1;
      string msg1 = "$have " + files_for_search[i];
      const char *fts = msg1.c_str();
      for(int j=0; j<num_nb; j++) {

         for(int x=0; x<100; x++) buff1[x] = 0;
         send(client_socket[j],fts,strlen(fts),0);
         recv(client_socket[j],buff1,100,0);

         int recv_id = atoi(buff1);

         if(recv_id > 0) {
            if(curr_id == 0) {
               curr_id = recv_id; sock_ind = j;
            }
            else{
               if(curr_id>recv_id) {
                  curr_id = recv_id; sock_ind = j;
               }
            }
         }        
      }
      if(curr_id>0) {
			string msg = "$sendFile " + files_for_search[i]; 
			send(client_socket[sock_ind],msg.c_str(),strlen(msg.c_str()),0);

			char buff3[100];
			for(int x=0; x<100; x++) buff3[x] = 0;
			int n, siz = 0;
			if ((n = recv(client_socket[sock_ind], buff3, 100, 0)) <0){
				perror("recv_size()");
				exit(errno);
			}

			siz = atoi(buff3);
         string ack("ack");
         send(client_socket[sock_ind],ack.c_str(),strlen(ack.c_str()),0);

			char Rbuffer[siz];
			   for(int x=0; x<siz; x++) Rbuffer[x] = 0;
			n = 0;
			int t=siz, pt =0;
			while(t > 0) {
				if ((n = recv(client_socket[sock_ind], Rbuffer+pt, siz, 0)) < 0){
				perror("recv_size()");
				exit(errno);
				}
				t-=n;
				pt+=n;           
			}
			FILE *final_file;
			string download_dir_path = dir_path + "Downloaded/";
			string file_path =  download_dir_path + files_for_search[i];       
			if(!directory_made) {
				mkdir(download_dir_path.c_str(),0777);
				directory_made = true;
			}

			final_file = fopen(file_path.c_str(), "wb");
			if (final_file != NULL) {
				fwrite(Rbuffer, sizeof(char), siz, final_file);
				fclose(final_file); 
			}
			cout << "Found "+files_for_search[i]+" at "+to_string(curr_id)+" with MD5 "+get_md5hash(file_path)+" at depth 1" << endl;
	   }
      else { 
         // asking neighbors to search for depth 2
         curr_id = 0;
		   int recv_id = 0,curr_port=0,recv_port;
         for(int j=0; j<num_nb; j++) {

            for(int x=0; x<100; x++) buff1[x] = 0;
            string msg2 = "$depth2ask " + files_for_search[i];
            send(client_socket[j],msg2.c_str(),strlen(msg2.c_str()),0);
            // receive uniq_id of neighbor-depth 2           
            recv(client_socket[j],buff1,100,0);
            string str_port_uniqId(buff1);
            stringstream ss(str_port_uniqId);
            ss >> recv_id;
            ss >> recv_port;
//cout << "Receiving DEPTHNB status "+to_string(recv_id)+"-"+to_string(recv_port)+" "+files_for_search[i] << endl;
            if(recv_id > 0) {
               if(curr_id == 0) {
                  curr_id = recv_id; curr_port = recv_port;
               }
               else{
                  if(curr_id>recv_id) {
                     curr_id = recv_id; curr_port = recv_port;
                  }
               }
            }
         }
         if (curr_id > 0) {
            int new_socket = socket(PF_INET,SOCK_STREAM,0);
            struct sockaddr_in serveraddr;
            memset(&serveraddr,0,sizeof(serveraddr));
            serveraddr.sin_family = AF_INET;
            //curr_port
            serveraddr.sin_port = htons(curr_port);
            serveraddr.sin_addr.s_addr = INADDR_ANY;

            int connec_sts = connect(new_socket,(struct sockaddr *) &(serveraddr),sizeof(serveraddr));
            // will come out of while loop on CONNECTING
            while( connec_sts == -1) {
               connec_sts = connect(new_socket,(struct sockaddr *) &(serveraddr),sizeof(serveraddr));
            }
            for(int x=0; x<100; x++) buff1[x] = 0;
            string connect_msg("0 0 0");
            send(new_socket,connect_msg.c_str(),strlen(connect_msg.c_str()),0);
            recv(new_socket,buff1,100,0);            

            string msg = "$CLOSEsendFile " + files_for_search[i]; 
            int a = send(new_socket,msg.c_str(),strlen(msg.c_str()),0);

            char buff3[100];
               for(int x=0; x<100; x++) buff3[x] = 0;
            int n, siz = 0;
//cout << "On depth2 ask+for: "+to_string(a)+" "+msg+ to_string(curr_port)<<endl;
            if ((n = recv(new_socket, buff3, 100, 0)) <0){
//cout << "recv_size()" << endl;               
               perror("recv_size()");
               exit(errno);
            }
//else if(n == 0){cout<< "On depth2 received 0 "+msg+" "+string(buff3)<<endl;}
//cout <<  "On depth2 received "+msg+" "+string(buff3)<<endl;
            siz = atoi(buff3);
            string ack("ack");
            send(new_socket,ack.c_str(),strlen(ack.c_str()),0);
//cout <<  "On depth2 received siz "+msg+" "+to_string(siz)<<endl;
            char Rbuffer[siz];
               for(int x=0; x<siz; x++) Rbuffer[x] = 0;
            n = 0;
            int t=siz, pt =0;
            while(t > 0) {
               if ((n = recv(new_socket, Rbuffer+pt, siz, 0)) < 0){
               perror("recv_size()");
               exit(errno);
               }
               t-=n;
               pt+=n;
            }

            FILE *final_file;
            string download_dir_path = dir_path + "Downloaded/";
            string file_path =  download_dir_path + files_for_search[i];       
            if(!directory_made) {
               mkdir(download_dir_path.c_str(),0777);
               directory_made = true;
            }

            final_file = fopen(file_path.c_str(), "wb");
            if (final_file != NULL) {
               fwrite(Rbuffer, sizeof(char), siz, final_file);
               fclose(final_file); 
            }
            cout << "Found "+files_for_search[i]+" at "+to_string(curr_id)+" with MD5 "+get_md5hash(file_path)+" at depth 2" << endl;
            close(new_socket);
         }                                 
         else{cout << "Found "+files_for_search[i]+" at 0 with MD5 0 at depth 0" << endl;}
      }      
   }
   /*
   string str_bye = "$BYE";
   for(int j=0; j<num_nb; j++) {
      send(client_socket[j],str_bye.c_str(),strlen(str_bye.c_str()),0);
      close(client_socket[j]);
   }      
   */
  string str_bye = "$BYE";
   char buff[50];
   for(int j=0; j<num_nb; j++) {
      send(client_socket[j],str_bye.c_str(),strlen(str_bye.c_str()),0);
      recv(client_socket[j],buff,50,0);
   }                       
   while(!allMyClients);

   string str_sbye = "$sBYE";
   for(int j=0; j<num_nb; j++) {
      send(client_socket[j],str_sbye.c_str(),strlen(str_sbye.c_str()),0);
      close(client_socket[j]);
   }                                                                                                     
}

void server_side(bool &allMyClients,int s_port,int s_id, int s_unique_id,vector<int> nb_num,vector<int> nb_port,int num_nb, vector<string> files_i_hv,string dir_path) {
   if(num_nb < 1) {
      return;
   }
   vector<string> nb_files;
   vector<int> nb_mapuniqId;
   vector<int> nb_mapport;
   fd_set master; // master file descriptor list
   fd_set read_fds;                 // temp file descriptor list for select()
   struct sockaddr_in remoteaddr;   // client address
   int fdmax;                       // maximum file descriptor number
   int listener;                    // listening socket descriptor
   int newfd;                       // newly accept()ed socket descriptor
   int yes = 1;

   socklen_t addrlen;
   string str_unique_id = to_string(s_unique_id);

   FD_ZERO(&master);
   FD_ZERO(&read_fds);              // clear the master and temp sets
   
   if ((listener = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
      perror("socket");
      exit(1);
   }
   
   if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
   }
   struct sockaddr_in server_addr;
   memset(&server_addr,0,sizeof(server_addr));
     
   server_addr.sin_family = AF_INET;
   server_addr.sin_port = htons(s_port);
   server_addr.sin_addr.s_addr = INADDR_ANY;
   memset(&(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));
   
   if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
      perror("bind");
      exit(1);
   }
   if (listen(listener,30)==-1){
      perror("listen");
      exit(1);
   }
   
   FD_SET(listener, &master);    // add the listener to the master set
   fdmax = listener;             // keep track of the biggest file descriptor

   int num_connected=0,num_byes = 0,num_sbyes = 0;
   string str_bye("$BYE"),str_sbye("$sBYE"),str_have("$have"),str_sendFile("$sendFile"),str_depth2ask("$depth2ask");
   string w1,w2,str_CLOSEsendFile("$CLOSEsendFile");
   //bool closeSockEndServer = false;
   int port, uniq_id, size_file;
   string name;
   char buff1[100],buff2[1024],buff_list[1024];
   vector<int> accepted_sockets;
   int r,res_bye,res_sbye,res_have,res_depth2ask,res_sendFile,res_CLOSEsendFile;

   struct timeval tv;
      tv.tv_sec = 4;
      tv.tv_usec = 0;

   allMyClients = false;

   for(;;) {      

      read_fds = master; // copy it
      if (select(fdmax+1, &read_fds, NULL, NULL, &tv) == -1) {
         perror("select");
         exit(1);
      }

      if(FD_ISSET(listener, &read_fds)) {
         addrlen = sizeof(remoteaddr);
         if ((newfd = accept(listener, (struct sockaddr*)&remoteaddr, &addrlen)) == -1) {
            perror("accept");
            exit(1);
         }
         else {
            FD_SET(newfd, &master); // add to master set
            accepted_sockets.push_back(newfd);
            //num_connected++; CHECK 3rd last COMMAND
            if (newfd > fdmax) fdmax = newfd;       
            for(int x=0; x<1024; x++) buff_list[x] = 0; 
            recv(newfd,buff_list,1024,0);
            send(newfd,str_unique_id.c_str(),strlen(str_unique_id.c_str()),0);            
            string big(buff_list);
            stringstream ss(big);
            ss >> port >> uniq_id >> size_file;
            for(int i=0; i<nb_port.size(); i++) {
               if (port == nb_port[i]) {num_connected++; break;}
            }
            for(int i=0; i<size_file; i++) {
               ss >> name;
               nb_files.push_back(name);
               nb_mapport.push_back(port);
               nb_mapuniqId.push_back(uniq_id);
            }
            
         }
      }
      if(num_connected < num_nb) continue;

      int accepted_size = accepted_sockets.size();
      for(int j=0;j<accepted_size;j++) {
         int i = accepted_sockets[j];
         if(FD_ISSET(i, &read_fds)) {
               // already established connections
               for(int x=0; x<100; x++) buff1[x] = 0;

               r = recv(i,buff1,100,0);           
               res_bye = strncmp(buff1,str_bye.c_str(),4);
               res_sbye = strncmp(buff1,str_sbye.c_str(),5);
               res_have = strncmp(buff1,str_have.c_str(),5);
               res_depth2ask = strncmp(buff1,str_depth2ask.c_str(),10);
               res_sendFile = strncmp(buff1,str_sendFile.c_str(),9);
               res_CLOSEsendFile = strncmp(buff1,str_CLOSEsendFile.c_str(),14);
               string line = string(buff1);
                  stringstream ss(line);
               if(res_bye == 0) {
                  num_byes++;
                  string ack("ack");
                  send(i,ack.c_str(),strlen(ack.c_str()),0);
                  if(num_byes == num_nb) {
                     allMyClients = true;
                  }
               }
               else if(res_sbye == 0) {
                  num_sbyes++;
                  if(num_sbyes == num_nb) {
                     for(int k=0;k<=fdmax;k++){
                        if(FD_ISSET(k, &master)){
                           FD_CLR(k,&master);
                           close(k);
                        }
                     }
                     return;
                  }
               }
               else if(res_have == 0) {        
                  // w1 : command, w2: filename
                  ss >> w1 >> w2; 
//cout << "Query: "+string(buff1)<< endl;
                  bool found = false;
                  for (int k=0; k<files_i_hv.size(); k++) {                  
                     int res = strcmp(w2.c_str(),files_i_hv[k].c_str());
                     if(res==0) {   
                        found = true;
                        break;
                     }
                  }
                  string msg;
                  if(found) {msg = to_string(s_unique_id);}
                  else {msg = "0";}
                  send(i,msg.c_str(),strlen(msg.c_str()),0);
               }
               else if(res_depth2ask == 0) {
                  // w1 : command, w2: filename
                  ss >> w1 >> w2; 

                  int curr_id = 0,recv_id,curr_port=0;
                  for(int k=0; k<nb_files.size(); k++) {
//cout << "Query: "+w2+" "+nb_files[k]<< endl;                     
                     if(w2.compare(nb_files[k])==0) {
                        recv_id = nb_mapuniqId[k];
                        if(recv_id > 0) {
                           if(curr_id == 0) {
                              curr_id = recv_id; curr_port = nb_mapport[k];
                           }
                           else{
                              if(curr_id>recv_id) {
                                 curr_id = recv_id; curr_port = nb_mapport[k];
                              }
                           }
                        }
                     }
                  }
                  string msg = to_string(curr_id)+" "+to_string(curr_port);
//cout << "Query: "+string(buff1)+" "+msg<< endl;
                  //string msg = server_asClient(person_uniqId,s_port,s_id,s_unique_id,nb_num,nb_port,num_nb,w2);                                           
                  send(i,msg.c_str(),strlen(msg.c_str()),0);
               }
               else if(res_sendFile == 0) {
                  // w1 : command, w2: filename
                  ss >> w1 >> w2; 
//cout << "Query: "+string(buff1)<< endl;
                  int n = 0, siz = 0;
                  string file_loc = dir_path + w2;
                  FILE *f;
                  char buff_for_file[100];
                  for(int x=0; x<100; x++) buff_for_file[x] = 0;
                  f = fopen(file_loc.c_str(),"rb");
                  fseek(f, 0, SEEK_END);
                  siz = ftell(f);
                  //sprintf(buff_for_file, "%d", siz);
                  string msg = to_string(siz);
                  // sending size of file
                  send(i,msg.c_str(),strlen(msg.c_str()),0);
                  
                  char Sbuf[siz];
                  recv(i,Sbuf,siz,0);
                  //Going to the beginning of the file
                  fseek(f, 0, SEEK_SET);

                  while(!feof(f)){
                     for(int x=0; x<siz; x++) Sbuf[x] = 0;
                     n = fread(Sbuf, sizeof(char), siz,f);
                     if (n > 0) { /* only send what has been read */
                        if((n = send(i, Sbuf, siz, 0)) < 0) {
                           perror("send_data()");
                           exit(errno);
                        }
                     }
                  }
//cout << "Succ Query: "+string(buff1)<< endl;
                  fclose(f);
               }
               else if(res_CLOSEsendFile == 0) {
                  // w1 : command, w2: filename
                  ss >> w1 >> w2; 
//cout << "Query: "+string(buff1)<< endl;
                  int n = 0, siz = 0;
                  string file_loc = dir_path + w2;
                  FILE *f;
                  char buff_for_file[100];
                  for(int x=0; x<100; x++) buff_for_file[x] = 0;
                  f = fopen(file_loc.c_str(),"rb");
                  fseek(f, 0, SEEK_END);
                  siz = ftell(f);
                  //sprintf(buff_for_file, "%d", siz);
                  string msg = to_string(siz);
                  // sending size of file
                  send(i,msg.c_str(),strlen(msg.c_str()),0);

                  char Sbuf[siz];
                  recv(i,Sbuf,siz,0);
                  //Going to the beginning of the file
                  fseek(f, 0, SEEK_SET);

                  while(!feof(f)){
                     for(int x=0; x<siz; x++) Sbuf[x] = 0;
                     n = fread(Sbuf, sizeof(char), siz,f);
                     if (n > 0) { /* only send what has been read */
                        if((n = send(i, Sbuf, siz, 0)) < 0) {
                           perror("send_data()");
                           exit(errno);
                        }
                     }
                  }
//cout << "Succ Query: "+string(buff1)<< endl;
                  fclose(f);
                  FD_CLR(i,&master);
                  accepted_sockets.erase(accepted_sockets.begin()+j);
                  close(i);
               }            
         }
      }
   }
}

int main(int argc, char* argv[]) {
   string config_file(argv[1]), dir_path(argv[2]), line, f;
   const char* cc_dirpath = dir_path.c_str();
   int port, id, unique_id, num_neigh, num_file, x, y;
   vector<string> f_name, files_with_me;
   vector<int> neigh_num, neigh_port;
   list_directory(cc_dirpath,files_with_me);

   ifstream read_config(config_file);

   getline(read_config, line);
   stringstream ss;
   ss << line;
   ss >> id >> port >> unique_id;

   getline(read_config, line);
   num_neigh = stoi(line);

   getline(read_config, line);
   if(num_neigh > 0) {
      stringstream ss(line);
      for(int i=0;i<num_neigh;i++) {
         ss >> x >> y;
         neigh_num.push_back(x);neigh_port.push_back(y);
      }
   }

   getline(read_config, line);
   num_file = stoi(line);

   if(num_file > 0) {
      for(int i=0;i<num_file;i++) {
         getline(read_config, line);
         f_name.push_back(line);
      }
      sort(f_name.begin(), f_name.end(), compare_strings);
   }

   bool allMyClients = false;
   thread c(client_side,ref(allMyClients),port,id,unique_id,neigh_num,neigh_port,num_neigh,f_name,files_with_me,dir_path);
   thread s(server_side,ref(allMyClients),port,id,unique_id,neigh_num,neigh_port,num_neigh,files_with_me,dir_path);
   s.join();
   c.join();
}

