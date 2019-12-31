#include <iostream>
#include <stdio.h>
#include<thread>
#include <algorithm>
#include <list>
#include <chrono>
#include <utility>
#include <vector>
#include <queue>
#include <mutex>
#include <deque>
using namespace std;


typedef struct f {
	int number =-1;
	int data =-1;
	int ack = -1;

} frame;

typedef struct timer_node
{
	frame  f;
	chrono::steady_clock::time_point t;
	bool finished = 0;

} timer_node;

queue <frame> frames_to_sender;
queue <frame> frames_to_reciever;
list <timer_node > timer_list;
int scenario_size;
std::mutex m1, m2,m3;

class physical_layer {

public:
	float prop_time; int BW; int time_out; 
	
	int window;

	 physical_layer(float prop_time, int BW , int window): prop_time(prop_time) ,BW(BW), window(window) {
	 time_out = 2 * prop_time * BW;
	}

	 void to_physical_layer_of_reciever(frame * f) 
	 {
		 
		 thread pipe(&physical_layer::physical_wire1,this, f);
		 pipe.detach();
	 }

	 void to_physical_layer_of_sender(frame * f)
	 {

		 thread pipe(&physical_layer::physical_wire2, this, f);
		 pipe.detach();
	 }


	 void physical_wire1(frame * f) {


		 std::this_thread::sleep_for(std::chrono::milliseconds(time_out / 2));

		 frame tmp = frame(*f);

		 m1.lock();
		frames_to_reciever.push(tmp);

		 m1.unlock();

		
	 }
	 void physical_wire2(frame * f) {


		 std::this_thread::sleep_for(std::chrono::milliseconds(time_out / 2));

		 frame tmp = frame(*f);

		 m2.lock();
		 frames_to_sender.push(tmp);

		 m2.unlock();


	 }

	 bool frame_recieved_from_sender(frame* f) {
		 m3.lock();
		 if (!frames_to_reciever.empty()) {
			 *f = frames_to_reciever.front();
			 frames_to_reciever.pop();
			 m3.unlock();
			 return true;
		 }
		 m3.unlock();
		 return false;

	 }


	 bool frame_recieved_from_reciever(frame* f) {
		 m3.lock();
		 if (!frames_to_sender.empty()) {
			 *f = frames_to_sender.front();
			 frames_to_sender.pop();
			 m3.unlock();
			 return true;
		 }
		 m3.unlock();
		 return false;

	 }
};


bool from_network_layer(frame* f) {

	static int i = 0;
	if (i<scenario_size)
	{
		frame a;
		a.data = 50;
		*f = a;
		i++;
		return true;
	}
	return false;

}


void reciever(physical_layer ph) {
	int next_expected_frame = 0;
	frame* f= new frame();
	
	while (1) {

		if (ph.frame_recieved_from_sender(f) ) 
		{
			if (f->number == next_expected_frame ) 
			{
				
				cout << "Reciever :recieved frame " << f->number << endl;
				f->ack = next_expected_frame;
				ph.to_physical_layer_of_sender(f);
				next_expected_frame=(next_expected_frame+1)%ph.window;
			}
			else 
				if (f->number <= next_expected_frame) //heuristic for case if acknowledge didn't reach to sender
				{

					cout << "Reciever :recieved redundant frame " << f->number <<"and discarded"<< endl;
					
				}

			
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	}

}

bool timeout(frame* f) {

	list<timer_node>::iterator it;

	for (it  = timer_list.begin();  it!= timer_list.end(); it++)
	{
		if (it->finished == true) {
			*f = it->f;
			timer_list.erase(it);
			return true;

		}
	}
	
	return false;

}

void sender(physical_layer ph) {
	int next_to_send = 0; int last_sent; frame * f = new frame();
	int next_ack_expected = 0;
	deque<frame> window;
	while (1) {
		
		if (from_network_layer(f) || next_to_send<scenario_size) {


			f->number = next_to_send;
			ph.to_physical_layer_of_reciever(f);
			cout << "Sender :sent frame " << f->number << "and waiting ack"<<endl;
			timer_node t; t.f = frame(*f); t.t = chrono::high_resolution_clock::now() + chrono::seconds(ph.time_out); t.finished = false;
			timer_list.push_back( t);
			next_to_send = (next_to_send + 1) % ph.window;
			last_sent = next_to_send;
		}
		
		if (timeout(f)) {

			if (f->number == next_ack_expected)
			{
			
				cout << "Sender :frame " << f->number << "timed out ....resending" << endl;

				ph.to_physical_layer_of_reciever(f);
				timer_node t; t.f = frame(*f); t.t = chrono::high_resolution_clock::now() + chrono::seconds(ph.time_out); t.finished = false;
				timer_list.push_back(t);

				next_to_send = (f->number + 1) % ph.window;

				
			}
			

		}




		if (ph.frame_recieved_from_reciever(f)) {

			if (f->ack ==next_ack_expected)
			{
				cout << "Sender :frame " << f->ack << " acknowleged"<<endl;
				next_ack_expected++;

			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

	}



}

void timer() {

	while (1) {
		for  (timer_node& el : timer_list)
		{
			chrono::steady_clock::time_point t = chrono::high_resolution_clock::now();
			if (chrono::duration_cast<chrono::milliseconds>(t - el.t).count() <0 ) {
	
				el.finished = true;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

}








int main() {

	scenario_size = 1;
	physical_layer ph(20, 20, 1000);
	
	thread t2(reciever, ph);
	thread t1(sender, ph);
	thread t3(timer);

	t1.detach();
	t2.detach();
	t3.detach();

	while (1) {};
	return 1;
}