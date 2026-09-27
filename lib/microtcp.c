/*
 * microtcp, a lightweight implementation of TCP for teaching,
 * and academic purposes.
 *
 * Copyright (C) 2015-2017  Manolis Surligas <surligas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "microtcp.h"
#include "../utils/crc32.h"


microtcp_sock_t
microtcp_socket (int domain, int type, int protocol)
{
  microtcp_sock_t micro_socket;

  micro_socket.sd =socket(domain,type,protocol);
  if(micro_socket.sd==-1){
      perror("UNABLE TO OPEN SOCKET\n");
      exit(1);
  }
  return micro_socket;
}

int
microtcp_bind (microtcp_sock_t *socket, const struct sockaddr *address,
               socklen_t address_len)
{
   if((socket-> sd)==-1 || address==NULL || address_len<=0){
    perror("UNABLE TO BIND");
    return-1;
  }

  int bind_ret=bind(socket->sd,address,address_len);
  if(bind_ret==-1){
      perror("UDP bind");
      return-1;
  }

  socket->state=LISTEN;
  //*socket =((address->sin_addr<<16) & 0xFFFF0000) | (address->sin_port & 0xFFFF0000)

  return 0;
}

int
microtcp_connect (microtcp_sock_t *socket, const struct sockaddr *address,
                  socklen_t address_len)
{
  int res,SYN_value;
  srand(time(NULL));
  uint64_t bytes_received=0;


  //initilliaze header values
  microtcp_header_t header;
  header.future_use0=0;
  header.future_use1=0;
  header.future_use2=0;
  header.ack_number=0;
  header.data_len=0;
  header.seq_number=rand();
  header.window=MICROTCP_WIN_SIZE;
  header.checksum=0;
  header.control=0;
  header.control=(uint16_t)SYN; //SYN=1
  

  //itilliaze the socket's fields
  socket->ack_number=0;
  socket->buf_fill_level=0;
  socket->bytes_lost=0;
  socket->bytes_send=0;
  socket->bytes_received=0;
  socket->curr_win_size=MICROTCP_WIN_SIZE;
  socket->init_win_size=MICROTCP_WIN_SIZE;
  socket->cwnd= MICROTCP_INIT_CWND;
  socket->packets_lost=0;
  socket->packets_received=0;
  socket->packets_send=0;
  socket->seq_number=0;
  socket->ssthresh=MICROTCP_INIT_SSTHRESH;
  socket->destination=(struct sockaddr*)malloc(200);
  if (socket->destination == NULL) {
      exit(1);
  };

  socket->destination=address;

  //bitwise operation to take the value of SYN
 // printf("ACK: %d\n",(header.control >> 12) & 1);
 // printf("RST: %d\n",(header.control >> 13) & 1);
 // printf("SYN: %d\n",(header.control >> 14) & 1);
 // printf("FYN: %d\n",(header.control >> 15) & 1);
 // printf("SEQ Client: %d \n", header.seq_number);
  //printf("SIZE HEAD: %ld\n",sizeof(header));

  socket->recvbuf=malloc(sizeof(uint8_t)*MICROTCP_RECVBUF_LEN);

  //SEND HEADER WITH SYN=1

  res=sendto(socket->sd,&header,sizeof(header),0,address,address_len);
  if(res==-1){
      perror("FAILED TO SEND MESSAGE");
      return-1;
  }
  socket->bytes_lost= sizeof(header)-res;
  //printf("BYTES LOST:%ld \n",socket->bytes_lost);

// printf("Header sent\n");

 //RECEIVE ACK WITH SERVER'S SYN=1

  if((bytes_received=recvfrom(socket->sd,socket->recvbuf,MICROTCP_RECVBUF_LEN,0,address,&address_len))==-1){ /*blocking call*/
        perror("FAILED TO RECEIVE MESSAGE");
        return-1;
  }

  if(bytes_received<=0){
    perror("EMPTY BUFFER");
    return-1;
  }

 // printf("Server's Header received\n");

  SYN_value=(((microtcp_header_t*)((socket->recvbuf)))->control >> 14) & 1;// check SYN flag

  if(SYN_value==1){

    //elegxos tou ACK 
    if(((microtcp_header_t*)(socket->recvbuf))->ack_number != header.seq_number + 1){
      socket->state=INVALID;
      return-1;
    }

    socket->state=ESTABLISHED;

    header.ack_number=((microtcp_header_t*)(socket->recvbuf))->seq_number+1;

    socket->ack_number=((microtcp_header_t*)(socket->recvbuf))->seq_number+1;

    header.control=(uint16_t)ACK; //ACK=1

    header.seq_number+=1; // for SYN
   
    socket->seq_number= header.seq_number;


    // SYN=0
    header.control=(uint16_t)(~(0b1 << 14) & 0xFFFF);
    
   // printf("NEW SYN: %d\n",(header.control >> 14) & 1); //bitwise operation to take the value of SYN
   // printf("NEW ACK: %d\n",(header.control >> 12) & 1);

    // SEND ACK
   // printf("Second Header sent/ ACK to SERVER\n");
   
    res=sendto(socket->sd,&header,sizeof(header),0,address,address_len);
    if(res==-1){
        perror("FAILED TO SEND MESSAGE");
        return-1;
    }
    socket->bytes_lost= sizeof(header)-res;
    //printf("BYTES LOST:%ld \n",socket->bytes_lost);

  }else{
    perror("NO SYN FLAG");
    return-1;
  }

  return 0;
}

int
microtcp_accept (microtcp_sock_t *socket, struct sockaddr *address,
                 socklen_t address_len)
{
  int res,SYN_value;
  size_t temp_ack;
  struct sockaddr_in client_address;
  socklen_t client_address_len=sizeof(struct sockaddr_in);
  srand(time(NULL));
  uint64_t bytes_received=0;

  //initilliaze header values 
  microtcp_header_t header;
  header.future_use0=0;
  header.future_use1=0;
  header.future_use2=0;
  header.ack_number=0;
  header.data_len=0;
  header.seq_number=rand();
  header.window=MICROTCP_WIN_SIZE;
  header.checksum=0;
  header.control=0;
  
  
  //itilliaze the socket's fields
  socket->ack_number=0;
  socket->buf_fill_level=0;
  socket->bytes_lost=0;
  socket->bytes_send=0;
  socket->bytes_received=0;
  socket->curr_win_size=MICROTCP_WIN_SIZE;
  socket->init_win_size=MICROTCP_WIN_SIZE;
  socket->cwnd= MICROTCP_INIT_CWND;
  socket->packets_lost=0;
  socket->packets_received=0;
  socket->packets_send=0;
  socket->seq_number=0;
  socket->ssthresh=MICROTCP_INIT_SSTHRESH;
  socket->recvbuf=malloc(sizeof(uint8_t)*MICROTCP_RECVBUF_LEN);
  socket->destination=(struct sockaddr*)malloc(200);
  if (socket->destination == NULL) {
     perror("Malloc failed");
     exit(1);
  };
 


 // printf("ACK: %d\n",(header.control >> 12) & 1);
 // printf("RST: %d\n",(header.control >> 13) & 1);
 // printf("SYN: %d\n",(header.control >> 14) & 1);
 // printf("FYN: %d\n",(header.control >> 15) & 1);
 // printf("SEQ Server: %d \n", header.seq_number);
 
  //RECEIVE first header with SYN=1 from client 
  if((bytes_received=recvfrom(socket->sd,socket->recvbuf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)&client_address,&client_address_len))==-1){ /*blocking call*/
      perror("FAILED TO RECEIVE MESSAGE");
      return -1;
  } 

  if(bytes_received<=0){
    perror("EMPTY BUFFER");
    return -1;
  }

  //address=(struct sockaddr*)&client_address;
  socket->destination=(struct sockaddr*)&client_address;

 // printf("1st Client's Header received \n");


  //printf("checksum:%d\n",((microtcp_header_t*)(socket->recvbuf))->checksum);
  SYN_value=(((microtcp_header_t*)(socket->recvbuf))->control >> 14) & 1;

  if(SYN_value==1){
    
    header.ack_number=((microtcp_header_t*)(socket->recvbuf))->seq_number+1;
    socket->ack_number=((microtcp_header_t*)(socket->recvbuf))->seq_number+1;

    header.control|=(uint16_t)ACK; //ACK=1
    header.control|=(uint16_t)SYN; //SYN=1
   
    
   // printf("NEW ACK: %d\n",(header.control >> 12) & 1);
   // printf("NEW RST: %d\n",(header.control >> 13) & 1);
   // printf("NEW SYN: %d\n",(header.control >> 14) & 1);
   // printf("NEW FYN: %d\n",(header.control >> 15) & 1);

    //SEND HEADER WITH SYN=1
   // printf("Header sent\n");
  
    res=sendto(socket->sd,&header,sizeof(header),0,(struct sockaddr*)&client_address,client_address_len);
    if(res==-1){
        perror("FAILED TO SEND MESSAGE");
        return -1;
    }

    socket->bytes_lost= sizeof(header)-res;
    //printf("BYTES LOST:%ld \n",socket->bytes_lost);
  }else{
    perror("NO SYN FLAG");
    return -1;
  }

  //Recieve ACK from client
  if((bytes_received=recvfrom(socket->sd,socket->recvbuf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)&client_address,&client_address_len))==-1){ /*blocking call*/
      perror("FAILED TO RECEIVE MESSAGE");
      return-1;
  }

  //printf("SIZE:%ld \n",socket->bytes_received);
  if(bytes_received<=0){
    perror("EMPTY BUFFER");
    return -1;
  }
 // printf("ACK from Client received\n");

  temp_ack=((microtcp_header_t*)(socket->recvbuf))->ack_number;

  if(temp_ack!= header.seq_number + 1){
    socket->state=INVALID;
    return-1;
  } 
  
   
  header.control=(uint16_t)(~(0b1 << 14) & 0xFFFF); //SYN=0
 // printf("NEW ACK: %d\n",(header.control >> 12) & 1);
 // printf("NEW SYN: %d\n",(header.control >> 14) & 1); 

  socket->seq_number=header.seq_number + 1; //FOR SYN
  header.seq_number+=1;
  socket->state=ESTABLISHED;
  
  //printf("debug!!!!!!!!:%hu \n",socket->destination->sa_family);
  // printf("Connection state: %d\n",socket->state);

  return 0;
  
}

int
microtcp_shutdown (microtcp_sock_t *socket, int how)
{
  int FIN_value, fin_packet, ack_packet,ACK_value;
  uint64_t bytes_received=0;
  struct sockaddr_in client_address;
  socklen_t address_len=sizeof(struct sockaddr_in);
  socklen_t client_address_len = sizeof(struct sockaddr_in);
  microtcp_header_t header;
  size_t temp_ack;
  struct timeval original_timeout ;
  original_timeout.tv_sec=0;
  original_timeout.tv_usec=0;  

    if (how == 1) { // server side
      if (socket->state == CLOSING_BY_PEER) { // the fin packet is already received
         
          header.seq_number = socket->seq_number;
          header.ack_number =socket->ack_number +1;
          header.control |= (uint16_t)ACK;
          header.window = socket->curr_win_size;
          header.data_len = 0;
          header.future_use0=0;
          header.future_use1=0;
          header.future_use2=0;
          header.checksum=0;


          // server sends ACK
          //printf("server:%hu \n",socket->destination->sa_family);

          //socket->destination->sa_family=2;
          if ((sendto(socket->sd, &header, sizeof(header), 0, socket->destination, address_len)) < 0) {
            socket->state = INVALID;
            perror("FAILED TO SEND ACK 1");
            exit(1);
          }
          
          header.seq_number = socket->seq_number;
          header.ack_number =socket->ack_number;
          header.control |= (uint16_t)FIN;
          header.window = socket->curr_win_size;
          header.data_len = 0;
          header.future_use0=0;
          header.future_use1=0;
          header.future_use2=0;
          header.checksum=0;

          // server sends FIN
          if ((sendto(socket->sd, &header, sizeof(header), 0, socket->destination, sizeof(struct sockaddr_in))) < 0) {
            socket->state = INVALID;
            perror("FAILED TO SEND FIN ");
            exit(1);
          }

          
          //receiving the final packet
          if((bytes_received=recvfrom(socket->sd,socket->recvbuf,MICROTCP_RECVBUF_LEN,0,socket->destination, &address_len))==-1) { 
            perror("FAILED TO RECEIVE PACKET");
            return-1;
          }

          // check ack
          ACK_value=(((microtcp_header_t*)(socket->recvbuf))->control >> 12) & 1;
          if (ACK_value!=1) {
            socket->state = INVALID;
            perror("Invalid ACK");
            exit(1);
          }
          temp_ack=((microtcp_header_t*)(socket->recvbuf))->ack_number;

          if(temp_ack != header.seq_number + 1){
            socket->state = INVALID;
            perror("WRONG ACK NUMBER");
            exit(1);
          }

       } else { // if smth in bandwidth test fails

          //recieve fin
          if(((bytes_received=recvfrom(socket->sd,socket->recvbuf,MICROTCP_RECVBUF_LEN,0,socket->destination, &address_len)))==-1){ 
              perror("FAILED TO RECEIVE MESSAGE");
              exit(1);
          }
          FIN_value=(((microtcp_header_t*)(socket->recvbuf))->control >> 15) & 1; // retrieve fin bit
          if (FIN_value!=1) {
            socket->state = INVALID;
            perror("Invalid FIN");
            exit(1);
          }

          header.seq_number = socket->seq_number;
          header.ack_number +=1;
          header.control |= (uint16_t)ACK;
          header.window = socket->curr_win_size;
          header.data_len = 0;
          header.future_use0=0;
          header.future_use1=0;
          header.future_use2=0;
          header.checksum=0;
          // server sends ACK
          if ((sendto(socket->sd, &header, sizeof(header), 0, socket->destination, sizeof(struct sockaddr_in))) < 0) {
            socket->state = INVALID;
            perror("FAILED TO SEND ACK ");
            exit(1);
          }

          socket->state = CLOSING_BY_PEER;

          
          header.seq_number = socket->seq_number;
          header.ack_number=socket->ack_number;
          header.control |= (uint16_t)FIN;
          header.window = socket->curr_win_size;
          header.data_len = 0;
          header.future_use0=0;
          header.future_use1=0;
          header.future_use2=0;
          header.checksum=0;
          // server sends FIN
          if ((sendto(socket->sd, &header, sizeof(header), 0, socket->destination, sizeof(struct sockaddr_in))) < 0) {
            socket->state = INVALID;
            perror("FAILED TO SEND FIN ");
            exit(1);
          }

          // receiving the final packet
          if((bytes_received=recvfrom(socket->sd,socket->recvbuf,MICROTCP_RECVBUF_LEN,0,socket->destination, &address_len))==-1) { 
            perror("FAILED TO RECEIVE PACKET");
            return-1;
          }

          // check ack
          ACK_value=(((microtcp_header_t*)(socket->recvbuf))->control >> 12) & 1;
          if (ACK_value!=1) {
            socket->state = INVALID;
            perror("Invalid ACK");
            exit(1);
          }
          temp_ack=((microtcp_header_t*)(socket->recvbuf))->ack_number;

          if(temp_ack != header.seq_number + 1){
            socket->state = INVALID;
            perror("WRONG ACK NUMBER");
            exit(1);
          }

      }

  } else { // client side - how = 0

      if (setsockopt(socket->sd, SOL_SOCKET, SO_RCVTIMEO, &original_timeout, sizeof(struct timeval)) < 0) {
              perror("setsockopt");
              // Handle the error...
       }

      //if smth bad happens in bamdwidth
      if (socket->state == CLOSING_BY_PEER) {

          
          // receive the ACK packet
          if((bytes_received=recvfrom(socket->sd,socket->recvbuf,MICROTCP_RECVBUF_LEN,0,socket->destination, &address_len))==-1) { 
              perror("FAILED TO RECEIVE ACK 1 MESSAGE");
              return-1;
          }
          ACK_value=(((microtcp_header_t*)(socket->recvbuf))->control >> 12) & 1; // retrieve ack bit
          if (ACK_value!=1) {
            socket->state = INVALID;
            perror("Invalid ACK");
            exit(1);
          }
          temp_ack=((microtcp_header_t*)(socket->recvbuf))->ack_number;

          if(temp_ack != header.seq_number + 1){
            socket->state = INVALID;
            perror("WRONG ACK NUMBER");
            exit(1);
          }

          socket->state = CLOSING_BY_HOST;

          // receive the FIN packet
          if((bytes_received=recvfrom(socket->sd,socket->recvbuf,MICROTCP_RECVBUF_LEN,0,socket->destination,  &address_len))==-1) { 
              perror("FAILED TO RECEIVE ACK 1 MESSAGE");
              return-1;
          }
          FIN_value=(((microtcp_header_t*)(socket->recvbuf))->control >> 15) & 1; // retrieve fin bit
          if (FIN_value!=1) {
            socket->state = INVALID;
            perror("Invalid FIN in cleint");
            exit(1);
          }
          

          // send final packet
          header.seq_number = htonl(((microtcp_header_t*)(socket->recvbuf))->ack_number);
          header.ack_number = ((microtcp_header_t*)(socket->recvbuf))->seq_number+1;
          header.control |= (uint16_t)ACK;
          header.window = socket->curr_win_size;
          header.data_len = 0;
          header.future_use0=0;
          header.future_use1=0;
          header.future_use2=0;
          header.checksum=0;

          if ((sendto(socket->sd, &header, sizeof(header), 0, socket->destination, sizeof(struct sockaddr_in))) < 0) {
            socket->state = INVALID;
            perror("FAILED TO SEND PACKET");
            exit(1);
          }

      } else { //first ever FIN

          //  send FIN
          header.seq_number = htonl(socket->seq_number+1);
          header.ack_number = socket->ack_number;
          header.control |= (uint16_t)FIN;
          header.window = socket->curr_win_size;
          header.data_len = 0;
          header.future_use0=0;
          header.future_use1=0;
          header.future_use2=0;
          header.checksum=0;
          struct sockaddr *dest=socket->destination;


          if ((sendto(socket->sd, &header, sizeof(header), 0, socket->destination, address_len)) < 0) {
            socket->state = INVALID;
            perror("FAILED TO SEND FIN PACKET");
            exit(1);
          }

          printf("\nFIN sent from client\n");
          // receive the ACK packet
          if((bytes_received=recvfrom(socket->sd,socket->recvbuf,MICROTCP_RECVBUF_LEN,0,dest, &address_len))==-1) { 
              perror("FAILED TO RECEIVE ACK 1 MESSAGE");
              return-1;
          }
         
          ACK_value=(((microtcp_header_t*)(socket->recvbuf))->control >> 12) & 1; // retrieve ack bit
          
          if (ACK_value!=1) {
            socket->state = INVALID;
            perror("Invalid ACK");
            exit(1);
          }
          temp_ack=((microtcp_header_t*)(socket->recvbuf))->ack_number;

          if(temp_ack != socket->seq_number + 1){
            socket->state = INVALID;
            perror("WRONG ACK NUMBER");
            exit(1);
          }

          socket->state = CLOSING_BY_HOST;

          // receive the FIN packet
          if((bytes_received=recvfrom(socket->sd,socket->recvbuf,MICROTCP_RECVBUF_LEN,0,socket->destination,  &address_len))==-1) { 
              perror("FAILED TO RECEIVE ACK 2 MESSAGE");
              return-1;
          }

          FIN_value=(((microtcp_header_t*)(socket->recvbuf))->control >> 15) & 1; // retrieve fin bit
          if (FIN_value!=1) {
            socket->state = INVALID;
            perror("Invalid FIN");
            exit(1);
          }
          

          //send final packet
          header.seq_number = htonl(((microtcp_header_t*)(socket->recvbuf))->ack_number);
          header.ack_number = ((microtcp_header_t*)(socket->recvbuf))->seq_number+1;
          header.control |= (uint16_t)ACK;
          header.window = socket->curr_win_size;
          header.data_len = 0;
          header.future_use0=0;
          header.future_use1=0;
          header.future_use2=0;
          header.checksum=0;

          if ((sendto(socket->sd, &header, sizeof(header), 0, socket->destination, sizeof(struct sockaddr_in))) < 0) {
            socket->state = INVALID;
            perror("FAILED TO SEND LAST PACKET");
            exit(1);
          }

      }
  }
 
  socket->state = CLOSED;
  
  free(socket->recvbuf);

  close(socket->sd);

  return 0;


}


ssize_t
microtcp_send (microtcp_sock_t *socket, const void *buffer, size_t length,
               int flags)
{
  microtcp_header_t header;
  microtcp_header_t* ACK_packet;
  int send=0,packets=0, i=0,chunks=0;
  socklen_t address_len=sizeof(struct sockaddr_in);
  int retransmission=0, TCPstate=1,sparing=0,dupACK=0; // 1 if retrans. is needed
  struct timeval timeout;
  timeout. tv_sec = 0;
  timeout. tv_usec = MICROTCP_ACK_TIMEOUT_US;
  uint64_t bytes_received=0,data_sent=0,bytes_to_send=0,offset=0,remaining=0,bytes_send=0;
  uint8_t *check_buf;
  uint8_t *receive_buf=malloc(MICROTCP_RECVBUF_LEN);


  if (setsockopt( socket->sd , SOL_SOCKET,SO_RCVTIMEO , & timeout ,sizeof( struct timeval)) < 0){
    perror("setsockopt");
  }


  if (socket->state == ESTABLISHED) {
      if (length <= 0) {
        perror("length <= 0!\n");
        free(receive_buf);
        return-1;
      }
   
    //// printf("     dest: %d\n",socket->destination->sa_family);
    // socket->destination->sa_family=2;
    //// printf("    new dest: %d\n",socket->destination->sa_family);
    header.seq_number =socket->seq_number;
    header.control|=(uint16_t)ACK;  // always ACK=1
    check_buf=malloc(MICROTCP_MSS);
    socket->bytes_send=0;

    remaining = length; //CHUNKSIZE=length 
   // printf("LENGTH: %ld\n",length);
    while( data_sent < length){
     
      offset+=data_sent; /*for sending all the chunks at a time*/
      bytes_to_send = min(socket->curr_win_size ,socket->cwnd ,remaining); 

      if(socket->curr_win_size==0){ //se priptwsei pou o buffer tou receiver gemisei
        while(socket->curr_win_size==0){
           // Initialize header values.       
          header.seq_number=socket->seq_number ;
          header.data_len=0;
          header.future_use0=0;
          header.future_use1 = 0; //4096
          header.ack_number = socket->ack_number;
        
          //allagh
          memcpy(check_buf,&header ,32 ); //data in header
          header.checksum = crc32(check_buf, 32); 

          srand(time(NULL));
          sleep(rand());
          // Send header.
          send = sendto(socket->sd, &header, sizeof(header), flags, socket->destination, address_len);
          if (send == -1) {
            perror("Header sending problem!\n");
            free(receive_buf);
            free(check_buf);
            return-1;
          }

          if ((bytes_received=recvfrom(socket->sd,receive_buf,MICROTCP_RECVBUF_LEN,0,socket->destination, &address_len)) > 0) {


          ACK_packet=((microtcp_header_t*)(receive_buf));

            if(ACK_packet->ack_number>=socket->seq_number){ // ordinary ack
              
               // printf("OK ACK!\n");
              socket->curr_win_size=ACK_packet->window;
              
            }
          }
        }
      }

     // printf("CURRENT WINDOW SIZE: %ld\n",socket->curr_win_size);
     // printf("bytes to sent: %ld\n",bytes_to_send);

      sparing=0;
      chunks = bytes_to_send / MICROTCP_MSS;
      header.future_use1 = bytes_to_send; //4096

      for(i = 0; i < chunks; i++){  /* MONO CHUNKS OF SIZE=MICROTCP_MSS*/

        // Initialize header values.       
        header.seq_number+= MICROTCP_MSS;
        header.data_len=MICROTCP_MSS;
        header.future_use0=length;
        header.future_use1 = bytes_to_send; //4096
        header.ack_number = socket->ack_number;
        //printf("header.seq_number sending chunks=%ld\n",header.seq_number);
      
       //allagh
        memcpy(check_buf, buffer+i*bytes_send+offset,header.data_len ); //data in 1 chunk
        header.checksum = crc32(check_buf, MICROTCP_MSS); 
        //printf("\ncheck sum in sent %ld\n",header.checksum);
      
        // Send header.
        send = sendto(socket->sd, &header, sizeof(header), flags, socket->destination, address_len);
        if (send == -1) {
          perror("Header sending problem!\n");
          free(receive_buf);
          free(check_buf);
          return-1;
        }

        
        // Send the data.
        send = sendto(socket->sd,check_buf, MICROTCP_MSS , flags, socket->destination, address_len);
        if (send == -1) {
          perror("Data sending problem!\n");
          free(receive_buf);
          free(check_buf);
          return-1;
        }

        bytes_send=MICROTCP_MSS; //gia na thymamai to size tou teleutaiou paketou pou esteila
          
        if(TCPstate==1){

          socket->cwnd*=2;  //slow start

          if(socket->cwnd>=socket->ssthresh){
            TCPstate = 2; //congestion avoidance fase
        
          }

        }else if(TCPstate==2 || TCPstate==3){
            socket->cwnd+=MICROTCP_MSS; // linear increase
        }

      }
      
     if( bytes_to_send % MICROTCP_MSS){ /* Check if there is a semi - filled chunck*/
        chunks++;  
        sparing=1;

       // printf("sparing chunk\n");
        // Initialize header values.
             
        header.ack_number = socket->ack_number;
        header.data_len = bytes_to_send % MICROTCP_MSS ;
        header.seq_number+=bytes_to_send % MICROTCP_MSS ;       
        header.future_use0=length;
        header.future_use1 = bytes_to_send; // bytes to send for now 4096 or less
       // printf("sparing header.data_len: %d",header.data_len);
        memcpy(check_buf, buffer+i*bytes_send+offset,header.data_len);       
        header.checksum = crc32(check_buf, bytes_to_send % MICROTCP_MSS);
        //printf("\nchecksum\n: %d",header.checksum);
        
        // Send header.

        //printf("header.seq_number in sent=%d\n",header.seq_number);
        send = sendto(socket->sd, &header, sizeof(header), flags, socket->destination, address_len);
        if (send == -1) {
          perror("Header sending problem!\n");
          free(receive_buf);
          free(check_buf);
          return-1;
        }

        // Send the data.
       
        send = sendto(socket->sd,check_buf, bytes_to_send % MICROTCP_MSS, flags, socket->destination, address_len);
        if (send == -1) {
          perror("Data sending problem!\n");
          free(receive_buf);
          free(check_buf);
          return-1;
        }

        bytes_send=header.data_len;
          
        if(TCPstate==1){

          socket->cwnd*=2;

          if(socket->cwnd>=socket->ssthresh){
            TCPstate = 2; //congestion avoidance fase
          }

        }else if(TCPstate==2){
            socket->cwnd+=MICROTCP_MSS; // linear increase
        }
      }

     // printf("chunks:%d \n",chunks);

     /* Get the ACKs */
      for(i = 0; i < chunks;i++){ 
            
        //memset(receive_buf,0,MICROTCP_RECVBUF_LEN); // to clear memory before using it

        //Check if the ACK received in some time.
        if ((bytes_received=recvfrom(socket->sd,receive_buf,MICROTCP_RECVBUF_LEN,0,socket->destination, &address_len)) < 0) {

          if (errno == EAGAIN || errno == EWOULDBLOCK) {

            // The operation was unblocked due to a timeout
            printf("\nrecv timeout\n");
            dupACK=0;
            retransmission=1; // like a flag
            timeout. tv_usec = MICROTCP_ACK_TIMEOUT_US*2;
            TCPstate=1; //slow start again 
            socket->ssthresh=socket->cwnd/2;
            socket->cwnd = min( MICROTCP_MSS , socket->ssthresh,400000000); /*trick*/

          } else {
            // Other error
            perror("recv error");
            free(receive_buf);
            free(check_buf);
            return -1;
          }

        }else if (bytes_received == 0) {
          perror("0 bytes received\n");
          free(receive_buf);
          free(check_buf);
          return -1;
          
        }else{  // Valid ACK
            //printf("debug1:%hu \n",socket->destination->sa_family);

            ACK_packet=((microtcp_header_t*)(receive_buf));

            if(ACK_packet->ack_number=socket->seq_number+ACK_packet->data_len){ // ordinary ack
               

               // printf("OK ACK!\n");
                socket->curr_win_size=ACK_packet->window;
                socket->packets_send++;
                socket->seq_number=ACK_packet->ack_number;
                socket->bytes_send+=ACK_packet->data_len;
              
                data_sent += ACK_packet->data_len;
                remaining -= ACK_packet->data_len;
               // printf("ACK PACKET DATA LENGTH: %d",ACK_packet->data_len);
               // printf("remainng: %ld\n",remaining);
               
              
            }else if(ACK_packet->ack_number>socket->seq_number){ //cumullative ack from timeout

               // printf("\nOK CUMULATIVE ACK!\n");

                socket->curr_win_size=ACK_packet->window;
                packets =  (ACK_packet->ack_number-socket->seq_number) / MICROTCP_MSS;
                for(i = 0; i < packets; i++){  
                  socket->packets_send++;
                } 
                 if((ACK_packet->ack_number-socket->seq_number) % MICROTCP_MSS){
                     socket->packets_send++;
                 }
              
                data_sent += ACK_packet->ack_number-socket->seq_number;
                remaining -= ACK_packet->ack_number-socket->seq_number;

                socket->seq_number=ACK_packet->ack_number;
                socket->bytes_send+=ACK_packet->ack_number-socket->seq_number;

            }else{ // dup ACK

             // printf("\nDuplicate Ack\n");

              dupACK++;

              if(dupACK==MAX_DUP_ACKS){
                  retransmission=2; // like a flag
                  socket->packets_lost++;
                  socket->bytes_lost=socket->seq_number - ACK_packet->ack_number;          
                  socket->ssthresh = socket->cwnd/2;
                  socket->cwnd = socket->cwnd/2 + 1;
                  dupACK=0;
              }
                  
            }
        }

        if(retransmission==1){ //from timeout

          header.ack_number = socket->ack_number;

          if(sparing=0){
            header.data_len = MICROTCP_MSS;
            header.seq_number+=MICROTCP_MSS ;

          }else if(i==chunks-1){
            header.data_len = bytes_to_send % MICROTCP_MSS ;
            header.seq_number+=bytes_to_send % MICROTCP_MSS ;
          }
                  
          header.future_use0=length;
          header.future_use1 = bytes_to_send; // bytes to send for now 4096 or less
          memcpy(check_buf, buffer+i*MICROTCP_MSS+offset,header.data_len);        
          header.checksum = crc32(check_buf, bytes_to_send % MICROTCP_MSS);
          //printf("\nchecksum\n: %d",header.checksum);
          
          // Send header.

          //printf("header.seq_number in sent=%d\n",header.seq_number);
          send = sendto(socket->sd, &header, sizeof(header), flags, socket->destination, address_len);
          if (send == -1) {
            perror("Header sending problem!\n");
            free(receive_buf);
            free(check_buf);
            return-1;
          }

          i--;
        }else if(retransmission==2){ //dup ack

          header.ack_number = socket->ack_number;

          if(sparing=0){ //there is not a spare chunk
            header.data_len = MICROTCP_MSS;
            header.seq_number+=MICROTCP_MSS ;

          }else if(i==chunks-1){
            header.data_len = bytes_to_send % MICROTCP_MSS ;
            header.seq_number+=bytes_to_send % MICROTCP_MSS ;
          }
                  
          header.future_use0=length;
          header.future_use1 = bytes_to_send; // bytes to send for now 4096 or less
          memcpy(check_buf, buffer+i*MICROTCP_MSS+offset,header.data_len);        
          header.checksum = crc32(check_buf, bytes_to_send % MICROTCP_MSS);
          //printf("\nchecksum\n: %d",header.checksum);
          
          // Send header.

          //printf("header.seq_number in sent=%d\n",header.seq_number);
          send = sendto(socket->sd, &header, sizeof(header), flags, socket->destination, address_len);
          if (send == -1) {
            perror("Header sending problem!\n");
            free(receive_buf);
            free(check_buf);
            return-1;
          }

          
          i--;
        }
        retransmission=0;

      }   
    }

    free(receive_buf);
    free(check_buf);
    return socket->bytes_send;
  } else {
    perror("The socket is not in established mode\n");
    return -1;
  }

  return 0;
}

ssize_t
microtcp_recv (microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
  microtcp_header_t ack_packet; 
  microtcp_header_t header;
  socklen_t address_len=sizeof(struct sockaddr_in);
  int i=0,chunks=0;
  int FIN_value;
  uint64_t bytes_received,bytes_sent=0,offset=0;
  uint8_t *receive_buf=malloc(MICROTCP_RECVBUF_LEN);
  uint32_t future1=0,checksum=0,total_length=0;
  ack_packet.control|=(uint16_t)ACK;  // always ACK=1
  struct sockaddr actual_dest;
  socket->bytes_received=0;
  memset(socket->recvbuf,0,MICROTCP_RECVBUF_LEN); // to clear memory for sure
  memset(receive_buf,0,MICROTCP_RECVBUF_LEN);

  //printf("debug0:%hu \n",socket->destination->sa_family);
  
  // 1ST EVER received header
  if((bytes_received=recvfrom(socket->sd,receive_buf,socket->curr_win_size,0,socket->destination, &address_len))==-1) { 
    perror("FAILED TO RECEIVE 1 HEADER ");
    return -1;
  }

  if(bytes_received<=0){ // header=32 bytes
    perror("EMPTY BUFFER");
    return -1;
  }

  actual_dest=*((struct sockaddr*)(socket->destination));
  //printf("debug_actual:%hu \n",socket->destination->sa_family);

  //edw sto mhxanhma styx petaxe seg fault
  memcpy(&header, receive_buf, sizeof(microtcp_header_t)); 
  
  //printf("debug333:%hu \n",actual_dest.sa_family);

  FIN_value=(header.control >> 15) & 1;
  socket->destination=(struct sockaddr*)&actual_dest;
  //printf("debug_actual:%hu \n",socket->destination->sa_family);

  if(FIN_value==1){
    socket->state=CLOSING_BY_PEER;
    return -1;
  }

  checksum=header.checksum;
  total_length=header.future_use0;
 // printf("total_length: %d\n",total_length);


  while(bytes_sent < CHUNK_SIZE){ //total length of mega chunk
    
    offset+=bytes_sent;
    chunks=header.future_use1/MICROTCP_MSS; //4096/1400
    if(header.future_use1%MICROTCP_MSS){ // for sparing bytes
      chunks++;
    }
    
   // printf("\nfuture1: %d\n",header.future_use1);
    future1=header.future_use1; //bytes the sender CAN send each time
   // printf("chunks in recv: %d\n",chunks);

    for(i = 0; i < chunks; i++){ //sending the acks for packets

      //lost packet
      if(header.seq_number>header.data_len + socket->ack_number){  
        
       // printf("Lost packet! %d\n",header.seq_number);         
        ack_packet.seq_number = socket->seq_number ;
        ack_packet.ack_number = socket->ack_number;

        if (sendto(socket->sd, &ack_packet, sizeof(ack_packet), flags, socket->destination, address_len) == -1) {
          perror("Header sending problem recv!\n");
          return-1;
        }


        // received header for thankfully correct packet
        if((bytes_received=recvfrom(socket->sd,receive_buf,socket->curr_win_size,0,socket->destination, &address_len))==-1) { 
          perror("FAILED TO RECEIVE 1 HEADER ");
          return -1;
        }

        if(bytes_received<=0){ // header=32 bytes
          perror("EMPTY BUFFER");
          return -1;
        }

        memcpy(&header, receive_buf, sizeof(microtcp_header_t));  // krataw to header NOT by ref
        i--; //gia na jana elegxw to pleon neo chunk
        break;

       // valid bytes
      }else if(header.seq_number==header.data_len + socket->ack_number){

        //receive DATA
       // printf("Data bytes in recev header: %d\n",header.data_len);
        if((bytes_received=recvfrom(socket->sd,socket->recvbuf,MICROTCP_RECVBUF_LEN,0,socket->destination, &address_len))==-1) { 
          perror("FAILED TO RECEIVE DATA 1");
          return -1;
        }
        if(bytes_received<=0){
          perror("EMPTY BUFFER");
          return -1;
        }

        // check header values
       
       
       // printf("Data bytes in header: %d\n",header.data_len);
       // printf("Bytes received in recv: %ld\n",bytes_received);
        //// printf("Ack number in  socket: %ld\n",socket->ack_number);
       // printf("Header seq_no received: %d\n",header.seq_number);   
       // printf("header: %d /received: %d\n",checksum,crc32(socket->recvbuf,header.data_len));

        if(checksum!=crc32(socket->recvbuf,header.data_len)){    
            perror("Wrong Checksum recv!");

            //send dup ACK 
              
            ack_packet.seq_number = socket->seq_number ;
            ack_packet.ack_number = socket->ack_number;

            if (sendto(socket->sd, &ack_packet, sizeof(ack_packet), flags, socket->destination, address_len) == -1) {
              perror("Header sending problem recv!\n");
              return-1;
            }


            // received header for the ,thankfully, correct packet
            if((bytes_received=recvfrom(socket->sd,receive_buf,socket->curr_win_size,0,socket->destination, &address_len))==-1) { 
              perror("FAILED TO RECEIVE 1 HEADER ");
              return -1;
            }

            if(bytes_received<=0){ // header=32 bytes
              perror("EMPTY BUFFER");
              return -1;
            }

            memcpy(&header, receive_buf, sizeof(microtcp_header_t));  // krataw to header NOT by ref
            i--; //gia na jana elegxw to pleon neo chunk
            break;
        }

        memcpy(((uint8_t*)buffer) + bytes_sent, socket->recvbuf, header.data_len); //combine all data together after getting every chunk
        
        
        // Send header ACK.
        ack_packet.ack_number=socket->ack_number + header.data_len;
        socket->ack_number= ack_packet.ack_number;
        socket->buf_fill_level+=header.data_len; // receiver's window   
        socket->packets_received++;
        socket->bytes_received+=header.data_len;  
        ack_packet.data_len=header.data_len;
       // printf("ack_packet.data_len= %d",ack_packet.data_len);
        ack_packet.seq_number=socket->seq_number; // same cause I send only ACK 
        ack_packet.window=MICROTCP_RECVBUF_LEN-socket->buf_fill_level; 

        bytes_sent+=header.data_len;

        // SEND ACK 
        if (sendto(socket->sd, &ack_packet, sizeof(ack_packet), flags, socket->destination, address_len) == -1) {
          perror("Header sending problem recv!\n");
          return -1;
        }

        
       // printf("ACK SENT\n");

      }else if(header.seq_number<=socket->ack_number){  //se periptwsh retransmit apo timeout enw exw lavei kanonika ta paketa mou

        // Send header.   
    
        ack_packet.data_len=header.data_len;
        ack_packet.ack_number=socket->ack_number;
        ack_packet.window=MICROTCP_RECVBUF_LEN-socket->buf_fill_level; //MICROTCP_RECVBUF_LEN-
        if (sendto(socket->sd, &ack_packet, sizeof(ack_packet), flags, socket->destination, address_len) == -1) {
          perror("Header sending problem recv!\n");
          return-1;
        }
       // printf("\nSENT ACK FROM MICRO-RECV AFTER SENDING ME THE SAME PACKET: %d\n", ack_packet.ack_number);

      }

     // printf("bytes_sent: %ld < header.future_use1: %d\n",bytes_sent,future1);
    
      if( i!= chunks-1 || (i==chunks-1  && ((bytes_sent <  future1 ) || ((bytes_sent = future1) && bytes_sent < total_length)))){  // otan den eimai sto teleutaio chunk h otan eimai alla den exoun teleivsei ta synolika bytes pou prepei nasteilei o user

       //RECEIVE NEXT HEADER
      
        memset(receive_buf,0,MICROTCP_RECVBUF_LEN); // to clear memory before using it
        if((bytes_received=recvfrom(socket->sd,receive_buf,socket->curr_win_size,0,socket->destination, &address_len))==-1) { 
          perror("FAILED TO RECEIVE NEXT HEADER");
          return -1;
        }

        if(bytes_received<=0){ // header=32 bytes
          perror("EMPTY BUFFER");
          return -1;
        }

        memcpy(&header, receive_buf, sizeof(microtcp_header_t));  // krataw to header NOT by ref

        FIN_value=(header.control >> 15) & 1;
        socket->destination=(struct sockaddr*)&actual_dest;
        //printf("debug:%hu \n",socket->destination->sa_family);
        if(FIN_value==1){
          
          memset(socket->recvbuf,0,MICROTCP_RECVBUF_LEN); // to clear memory before using it
          socket->state=CLOSING_BY_PEER;
          return -1;
        }

        checksum=header.checksum;
        total_length=header.future_use0;
        //printf("header: %d\n",checksum);

      }else{
        free(receive_buf);
        bytes_sent=CHUNK_SIZE;
      }
    }
  }
  socket->buf_fill_level-=total_length;
 // printf("\nocket->bytes_received %ld\n",socket->bytes_received);
  return socket->bytes_received;
}




// min data to sent each time
size_t min(size_t rwnd,size_t cwnd, size_t remaining_bytes){

  size_t bytes=0;

  if(rwnd<=cwnd && rwnd <=remaining_bytes){
    bytes=rwnd;
  }else if(cwnd<=remaining_bytes && cwnd<=rwnd){
    bytes=cwnd;
  }else if(remaining_bytes<=rwnd && remaining_bytes<=cwnd){
    bytes=remaining_bytes;
  }

  return bytes;
}