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
#include <sys/stat.h>
#include <openssl/md5.h>
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
   if (files_with_me.size()>0) {
      sort(files_with_me.begin(), files_with_me.end(), compare_strings);
      for(int i=0; i<files_with_me.size(); i++) {cout << files_with_me[i] << endl;}
   }
}

void client_side(int c_port,int c_id,int c_unique_id,vector<int> nb_num,vector<int> nb_port,int num_nb, vector<string> files_for_search, string dir_path) {
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
   for(int i=0;i<num_nb;i++)  {
      cnctd[i] = 0;
      struct sockaddr_in serveraddr;
      memset(&serveraddr,0,sizeof(serveraddr));
      serveraddr.sin_family = AF_INET;
      serveraddr.sin_port = htons(nb_port[i]);
      serveraddr.sin_addr.s_addr = INADDR_ANY;

      server_nb_addr.push_back(serveraddr);
   }
   char buff1[1024];   //server response

   while(num_connected < num_nb) { 
      for(int i=0;i<num_nb;i++) {    
         if(cnctd[i]==0) {         
            int connec_sts = connect(client_socket[i],(struct sockaddr *) &(server_nb_addr[i]),sizeof(server_nb_addr[i]));
            if(connec_sts == -1) continue;
            else {
               cnctd[i] =1;
               num_connected++;
               for(int x=0; x<1024; x++) buff1[x] = 0;
               recv(client_socket[i],buff1,1024,0);
               nb_uniqId[i] = atoi(buff1);
            }        
         }       
      }
   }
   
   for(int i=0;i<num_nb;i++) {
      cout << "Connected to " + to_string(nb_num[i]) + " with unique-ID " + to_string(nb_uniqId[i]) + " on port " + to_string(nb_port[i]) << endl;
   } 


   bool directory_made = false;
   for(int i=0; i<files_for_search.size(); i++) {
      string msg = "$have " + files_for_search[i]; 
      int curr_id = 0, sock_ind = -1;

      for(int j=0; j<num_nb; j++) {
         char buff[100];
         for(int x=0; x<100; x++) buff[x] = 0;
         send(client_socket[j],msg.c_str(),strlen(msg.c_str()),0);
         recv(client_socket[j],buff,100,0);

         int recv_id = atoi(buff);
        
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
      if(curr_id==0) {cout << "Found "+files_for_search[i]+" at 0 with MD5 0 at depth 0" << endl;}
      else { 
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
         char Rbuffer[siz];
            for(int x=0; x<siz; x++) Rbuffer[x] = 0;
         string ack("ack");
         send(client_socket[sock_ind],ack.c_str(),strlen(ack.c_str()),0);
         
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
//cout << "File Received : " + files_for_search[i] << endl;
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
   }
   
   for(int j=0; j<num_nb; j++) {
      string str_bye = "$BYE";
      const char* cc_bye = str_bye.c_str();
      send(client_socket[j],cc_bye,strlen(cc_bye),0);
      close(client_socket[j]);
   }                                                                                                                         
}

void server_side(int s_port,int s_id, int s_unique_id,vector<int> nb_num,vector<int> nb_port,int num_nb, vector<string> files_i_hv, string dir_path) {
   fd_set master; // master file descriptor list
   fd_set read_fds;                 // temp file descriptor list for select()
   struct sockaddr_in remoteaddr;   // client address
   int fdmax;                       // maximum file descriptor number
   int listener;                    // listening socket descriptor
   int newfd;                       // newly accept()ed socket descriptor
   int yes = 1;

   socklen_t addrlen;
   string str_unique_id = to_string(s_unique_id);
   const char* buff1 = str_unique_id.c_str();

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
   if (listen(listener,10)==-1){
      perror("listen");
      exit(1);
   }
   
   FD_SET(listener, &master);    // add the listener to the master set
   fdmax = listener;             // keep track of the biggest file descriptor

   int num_byes = 0;
   string str_bye("$BYE"), str_have("$have"), str_sendFile("$sendFile"),w1,w2;
   char buff2[1024];

   struct timeval tv;

      tv.tv_sec = 2;
      tv.tv_usec = 0;

   for(;;) {      

      read_fds = master; // copy it
      if (select(fdmax+1, &read_fds, NULL, NULL, &tv) == -1) {
         perror("select");
         exit(1);
      }

      for(int i=0;i<=fdmax;i++) {
         
         for(int x=0; x<1024; x++) buff2[x] = 0;

         if(FD_ISSET(i, &read_fds)) {
            if(i==listener) {
               addrlen = sizeof(remoteaddr);
               if ((newfd = accept(listener, (struct sockaddr*)&remoteaddr, &addrlen)) == -1) {
                  perror("accept");
               }
               else {
                  FD_SET(newfd, &master); // add to master set
                  if (newfd > fdmax) fdmax = newfd;            
                  send(newfd,buff1,strlen(buff1),0);
               }
            }
            else {
               // already established connections
               recv(i,buff2,1024,0);               
               int res_bye = strncmp(buff2,str_bye.c_str(),4);
               int res_have = strncmp(buff2,str_have.c_str(),5);
               int res_sendFile = strncmp(buff2,str_sendFile.c_str(),9);
               if(res_bye == 0) {
                  num_byes++;
                  FD_CLR(i, &master); 
                  close(i);
                  if(num_byes == num_nb) {close(listener); return;}
                  continue;
               }
               else {
                  string line = string(buff2);
                  stringstream ss(line);
                  ss >> w1 >> w2;
                  if(res_have == 0) {                     
                     bool found = false;
                     for (int j=0; j<files_i_hv.size(); j++) {                  
                        if(w2.compare(files_i_hv[j])==0) {   
                           found = true;
                           break;
                        }
                     }
                     string msg;
                     if(found) {msg = to_string(s_unique_id);}
                     else {msg = "0";}
                     send(i,msg.c_str(),strlen(msg.c_str()),0);
                  }
                  else if(res_sendFile == 0) {
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
                     // useless ACK
                     recv(i,buff_for_file,100,0);

                     char Sbuf[siz];
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
                     fclose(f);
                  }
               }
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
   thread s(server_side,port,id,unique_id,neigh_num,neigh_port,num_neigh,files_with_me,dir_path);
   thread c(client_side,port,id,unique_id,neigh_num,neigh_port,num_neigh,f_name,dir_path);
   s.join();
   c.join();
}
