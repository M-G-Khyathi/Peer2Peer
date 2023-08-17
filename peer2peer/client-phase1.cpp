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

using namespace std;

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

void list_directory(const char *dir_path) {
   vector<string> files_in_order;
   struct dirent *file;
   DIR *dir = opendir(dir_path);
   if (dir == NULL) {
      return;
   }

   while ((file = readdir(dir)) != NULL) {
        string f_name = string(file->d_name);
        if (!(f_name.compare(".") == 0 || f_name.compare("..") == 0 || f_name.compare("Downloaded") == 0)) {
            files_in_order.push_back(f_name);
        }  
   }
   closedir(dir);
   if (files_in_order.size()>0) {
      sort(files_in_order.begin(), files_in_order.end(), compare_strings);
      for(int i=0; i<files_in_order.size(); i++) {cout << files_in_order[i] << endl;}
   }
}

void client_side(int c_port,int c_id,int c_unique_id,vector<int> nb_num,vector<int> nb_port,int num_nb) {
   if (num_nb < 1) return;
   // above code is important.
   vector<int> client_socket, nb_uniqId(num_nb);
   int num_connected=0;
   for(int i=0;i<num_nb;i++) {
      client_socket.push_back(socket(PF_INET,SOCK_STREAM,0));
   }
   vector<sockaddr_in> server_nb_addr;
   bool cnctd[num_nb];
   char buff[1024];   //server response

   for(int i=0;i<num_nb;i++) {
      cnctd[i] = 0;
      struct sockaddr_in serveraddr;
      memset(&serveraddr,0,sizeof(serveraddr));
      serveraddr.sin_family = AF_INET;
      serveraddr.sin_port = htons(nb_port[i]);
      serveraddr.sin_addr.s_addr = INADDR_ANY;
      server_nb_addr.push_back(serveraddr);
   }

   while(num_connected < num_nb) { 
      for(int i=0;i<num_nb;i++) {     
         if(cnctd[i]==0) {         
            int connec_sts = connect(client_socket[i],(struct sockaddr *) &(server_nb_addr[i]),sizeof(server_nb_addr[i]));
            if(connec_sts == -1) continue;
            else {
               cnctd[i] =1;
               num_connected++;
               recv(client_socket[i],buff,1024,0);
               nb_uniqId[i] = atoi(buff);
            }        
         }       
      }
   }
   
   for(int i=0;i<num_nb;i++) {
      cout << "Connected to " + to_string(nb_num[i]) + " with unique-ID " + to_string(nb_uniqId[i]) + " on port " + to_string(nb_port[i]) << endl;
   } 
   for(int j=0; j<num_nb; j++) {
      string str_bye = "$BYE";
      const char* cc_bye = str_bye.c_str();
      send(client_socket[j],cc_bye,strlen(cc_bye),0);
      close(client_socket[j]);
   }
   return;                                                                                                                                 
}

void server_side(int s_port,int s_id, int s_unique_id,vector<int> nb_num,vector<int> nb_port,int num_nb) {
   if(num_nb < 1) return;
   fd_set master; // master file descriptor list
   fd_set read_fds;                 // temp file descriptor list for select()
   struct sockaddr_in remoteaddr;   // client address
   int fdmax;                       // maximum file descriptor number
   int listener;                    // listening socket descriptor
   int newfd;                       // newly accept()ed socket descriptor
   int yes = 1;
   socklen_t addrlen;
   string msg = to_string(s_unique_id);
   const char *buff = msg.c_str();

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
   for(;;) {
      struct timeval tv;

      tv.tv_sec = 2;
      tv.tv_usec = 0;

      read_fds = master; // copy it
      if (select(fdmax+1, &read_fds, NULL, NULL, &tv) == -1) {
         perror("select");
         exit(1);
      }

      for(int i=0;i<=fdmax;i++) {
         char buff2[1024];
         for(int x=0; x<1024; x++) buff2[x] = '\0';

         if(FD_ISSET(i, &read_fds)) {
            if(i==listener) {
               addrlen = sizeof(remoteaddr);
               if ((newfd = accept(listener, (struct sockaddr*)&remoteaddr, &addrlen)) == -1) {
                  perror("accept");
               }
               else {
                  FD_SET(newfd, &master); // add to master set
                  if (newfd > fdmax) fdmax = newfd;
                  send(newfd,buff,strlen(buff),0);
               }
            }
            else {
               // already established connections
               recv(i,buff2,1024,0);

               string str_bye("$BYE");
               int res_bye = strcmp(buff2,str_bye.c_str());
               if(res_bye == 0) {
                  num_byes++;
                  FD_CLR(i, &master); 
                  close(i);
                  if(num_byes == num_nb) {close(listener); return;}
                  continue;
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
   vector<string> f_name;
   vector<int> neigh_num, neigh_port;
   list_directory(cc_dirpath);

   ifstream read_config(config_file);

   getline(read_config, line);
   stringstream ss;
   ss << line;
   ss >> id >> port >> unique_id;
   
   getline(read_config, line);
   num_neigh = stoi(line);

   getline(read_config, line);
   if(num_neigh > 0) {
      stringstream ss;
      ss << line;
      for(int i=0;i<num_neigh;i++) {
         ss >> x >> y;
         neigh_num.push_back(x);neigh_port.push_back(y);
      }
   }

   getline(read_config, line);
   num_file = stoi(line);

   if (num_file > 0) {
      for(int i=1;i<=num_file;i++) {
         getline(read_config, line);
         f_name.push_back(line);
      }
      sort(f_name.begin(), f_name.end(), compare_strings);
   }
   
   thread s(server_side,port,id,unique_id,neigh_num,neigh_port,num_neigh);
   thread c(client_side,port,id,unique_id,neigh_num,neigh_port,num_neigh);
   s.join();
   c.join();
}
