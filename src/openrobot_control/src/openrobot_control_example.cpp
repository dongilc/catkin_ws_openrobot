/*
 * openrobot_control_example_node.cpp
 * 
 *  Created on: Jan. 07, 2021
 *      Author: cdi
 */

#include "openrobot_control.h"

// Settings
#define VESC_UART_DEV1			"/dev/ttyACM1"
#define VESC_UART_DEV2			"/dev/ttyACM2"
#define VESC_UART_DEV3			"/dev/ttyACM3"
#define NO_OF_UDEV				3				// Max. UART Device number : 3
#define CAN_FORWARD_OFF			0
#define CAN_FORWARD_ON			1
#define BRAKE_CURRENT			10.
#define BRAKE_THRESHOLD			8.	// bigger than (BRAKE_CURRENT/2)

// COMM_SET Types - VESC Original
#define COMM_ALIVE				30
#define COMM_SET_DUTY			5
#define COMM_SET_CURRENT		6
#define COMM_SET_CURRENT_BRAKE	7
#define COMM_SET_RPM			8
#define COMM_SET_POS			9
#define COMM_SET_HANDBRAKE		10

// OpenRobot App
#define TARGET_VESC_ID_LOCAL	255

// OpenRobot Custom COMM_SET Types
typedef enum {
	COMM_SET_RELEASE = 100,
	COMM_SET_DPS,
	COMM_SET_DPS_VMAX,
	COMM_SET_DPS_AMAX,
	COMM_SET_SERVO,
	COMM_SET_TRAJ
} COMM_PACKET_ID_OPENROBOT;

// Omniwheel Robot Platform
typedef enum {
	DISABLED = 0,
	DUTY_MODE,
	DPS_MODE
} CONTROL_MODE;

// Conversions
#define RAD2DEG         		180.0/M_PI  // radian to deg
#define RPS2DPS					RAD2DEG	

// Uncomment this only when you want to see the below infomations.
//#define PRINT_SENSOR_CORE
#define PRINT_SENSOR_CUSTOMS

void TeleopInput::keyboardCallback(const geometry_msgs::Twist::ConstPtr& cmd_vel)
{
	/*
	//ROS_INFO("lin x:%.2f, y:%.2f, z:%.2f", cmd_vel->linear.x, cmd_vel->linear.y, cmd_vel->linear.z);
	//ROS_INFO("ang x:%.2f, y:%.2f, z:%.2f", cmd_vel->angular.x, cmd_vel->angular.y, cmd_vel->angular.z);
	dps[0] = cmd_vel->linear.x*2.;//cmd_vel->linear.x*200.;
	speed[0] = cmd_vel->angular.z*1.;
	if(cmd_vel->angular.z < 0) {
		enable.data = false;
		dps[0] = 0.;
		speed[0] = 0.;
	}
	else 
	{
		startTime = ros::Time::now();
		enable.data = true;
	}
	*/
}

void TeleopInput::joyCallback(const sensor_msgs::Joy::ConstPtr& joy)
{
	static int joy_cont_mode;
	double joy_cmd_forward, joy_cmd_lateral, joy_cmd_steering, joy_cmd_brake;

	// Logitech F710 Controller
	//ROS_INFO("%.3f, %.3f, %.3f, %.3f", joy->axes[0], joy->axes[1], joy->axes[2], joy->axes[3]);
	//ROS_INFO("%d, %d", joy->buttons[0], joy->buttons[1]);	// 0, 1, 2, 3 : X, A, B, Y

	joy_cmd_forward = -(joy->axes[0])*(5.5);
	joy_cmd_lateral = (joy->axes[3])*(1.5);
	joy_cmd_steering = (joy->axes[1])*(3.5);

	vh1_->speed[0] = joy_cmd_forward;
	vh1_->speed[1] = joy_cmd_lateral;
	vh1_->speed[2] = joy_cmd_steering;

	// control enable / disable
	if(joy->buttons[0]==0 && joy_cont_mode != DISABLED) {
		joy_cont_mode = DISABLED;
		vh1_->enable.data = false;
		ROS_INFO("Control Disabled");
	}
	else if(joy->buttons[0]==1 && joy->buttons[1]==0 && joy_cont_mode!=DUTY_MODE) {
		joy_cont_mode = DUTY_MODE;
		vh1_->custom_cmd_type[0] = vh1_->custom_cmd_type[1] = vh1_->custom_cmd_type[2] = vh1_->custom_cmd_type[3] = COMM_SET_DUTY;
		vh1_->enable.data = true;
		ROS_INFO("Duty Control Mode On");
	}
	else if(joy->buttons[0]==1 && joy->buttons[1]==1 && joy_cont_mode!=DPS_MODE) {
		joy_cont_mode = DPS_MODE;
		vh1_->custom_cmd_type[0] = vh1_->custom_cmd_type[1] = vh1_->custom_cmd_type[2] = vh1_->custom_cmd_type[3] = COMM_SET_DPS;
		vh1_->enable.data = true;
		ROS_INFO("DPS Control Mode On");
	}
}

void TrapezoidalVelProfile::GenProfile(float v_ref, float *vout)
{
	float da = 0;
	float dv = 0;

	// Profile Deg
	if(v_ref == *vout) {
		dv = 0;
	}
	else {
		da = (v_ref - *vout)/dt;
		if(fabs(da) >= (double)Amax) {
			if(da>0) da = Amax;
			else 	 da = -Amax;
		}
	}
	dv = da*dt;
	*vout += dv;

	//ROS_INFO("dv:%.2f", dv);
}

void TeleopVesc::customsCallback(const openrobot_vesc_msgs::VescGetCustomApp::ConstPtr& custom_rx_msg)
{
#ifdef PRINT_SENSOR_CUSTOMS
	int vesc_dev_number = 1;
	vesc_dev_number = custom_rx_msg->can_devs_num + 1;

	ROS_INFO("------------------------------------------");
	ROS_INFO("header:%6.4f", custom_rx_msg->header.stamp.toSec());
	ROS_INFO("vesc dev number:%d", vesc_dev_number);
	
	for(int i=0; i<vesc_dev_number; i++) {
		ROS_INFO("---------< can id : %d >---------", custom_rx_msg->can_id[i]);
		ROS_INFO("voltage input:%.1f V", custom_rx_msg->voltage_input[i]);
		ROS_INFO("temperature pcb:%.1f C", custom_rx_msg->temperature_pcb[i]);
		ROS_INFO("temperature motor:%.1f C", custom_rx_msg->temperature_motor[i]);
		ROS_INFO("current motor:%.2f A", custom_rx_msg->current_motor[i]);
		ROS_INFO("current input:%.2f A", custom_rx_msg->current_input[i]);
		ROS_INFO("watt hours:%.4f Wh", custom_rx_msg->watt_hours[i]);
		ROS_INFO("watt hours charged:%.4f Wh", custom_rx_msg->watt_hours_charged[i]);
		ROS_INFO("duty:%.3f", custom_rx_msg->duty_cycle[i]);
		ROS_INFO("accum. deg now:%.2f deg", custom_rx_msg->accum_deg_now[i]);
		ROS_INFO("rps now:%.2f", custom_rx_msg->diff_deg_now[i]);
    }
#endif
}

void TeleopVesc::stateCallback(const openrobot_vesc_msgs::VescStateStamped::ConstPtr& state_msg)
{
#ifdef PRINT_SENSOR_CORE
	ROS_INFO("------------------------------------------");
	ROS_INFO("header:%6.4f", state_msg->header.stamp.toSec());
	ROS_INFO("voltage input:%.2f V", state_msg->state.voltage_input);
	ROS_INFO("temperature pcb:%.2f C", state_msg->state.temperature_pcb);
	ROS_INFO("current motor:%.2f A", state_msg->state.current_motor);
	ROS_INFO("current input:%.2f A", state_msg->state.current_input);
	ROS_INFO("erpm:%.2f", state_msg->state.speed);
	ROS_INFO("duty:%.2f", state_msg->state.duty_cycle);
	ROS_INFO("amp_hours:%.2f", state_msg->state.charge_drawn);
	ROS_INFO("amp_hours_charged:%.2f", state_msg->state.charge_regen);
	ROS_INFO("watt_hours:%.2f", state_msg->state.energy_drawn);
	ROS_INFO("watt_hours_charged:%.2f", state_msg->state.energy_regen);
	ROS_INFO("tachometer:%.2f", state_msg->state.displacement);
	ROS_INFO("tachometer_abs:%.2f", state_msg->state.distance_traveled);
	ROS_INFO("fault code:%d", state_msg->state.fault_code);
	ROS_INFO("pid_pos_now:%.2f", state_msg->state.pid_pos_now);
	ROS_INFO("controller_id:%d", state_msg->state.controller_id);
#endif
}

void TeleopVesc::setCmdMsg(double data, int send_can, int can_id)
{
	cmd_msg.data = data;
	cmd_msg.send_can = send_can;
	cmd_msg.can_id = can_id;
}

void TeleopVesc::setCustomMsg(int can_id, int send_can, int cmd_type, double data)
{
	//
	custom_tx_msg.id_set.push_back(can_id);
	custom_tx_msg.can_forward_set.push_back(send_can);
	custom_tx_msg.comm_set.push_back(cmd_type);
	custom_tx_msg.value_set.push_back(data);
}

void TeleopVesc::requestCustoms()
{
	std_msgs::Bool msg;
	msg.data = true;
	vesc_cmd_get_customs.publish(msg);
}

void TeleopVesc::setCustomOut()
{
	int num_of_id = 0;
	int can_forw = 0;

	// Clear Custom Message
	custom_tx_msg.id_set.clear();
	custom_tx_msg.can_forward_set.clear();
	custom_tx_msg.comm_set.clear();
	custom_tx_msg.value_set.clear();

	// Custom Command
	for(int i=0; i<this->NO_VESC; i++) {
		if(i==0) can_forw = CAN_FORWARD_OFF;
		else     can_forw = CAN_FORWARD_ON;
		setCustomMsg(controller_id[i], can_forw, custom_cmd_type[i], custom_cmd_value[i]);
		num_of_id++;
	}
	custom_tx_msg.num_of_id = num_of_id;
	custom_tx_msg.data_bytes = 2 + 6*num_of_id;
	vesc_cmd_set_customs.publish(custom_tx_msg);
}

/*
 * Main Function
 * 
 */
int main(int argc, char **argv)            
{
  ros::init(argc, argv, "vesc_control_node");

  // loop freq
//   int rate_hz = 500;	//hz
  int rate_hz = 50;	//hz
  uint32_t cnt = 0;

  ros::NodeHandle nh("~");
  std::string port;
  int no_vesc;
  TeleopVesc *teleop_vesc[NO_OF_UDEV];	// TeleopVesc Class	

  // TeleopVesc Class
  for(int i=0; i<NO_OF_UDEV; i++)
  {
	std::string p = "port" + std::to_string(i);
	std::string v = "no_vesc" + std::to_string(i);

	if(nh.getParam(p, port) && nh.param(v, no_vesc, 1)) {
		teleop_vesc[i] = new TeleopVesc(no_vesc, port);
		ROS_INFO("VESC Driver %d activated at port:%s, number of vesc:%d", i, port.c_str(), no_vesc);
	}
  }

  // TeleInput Class	
  TeleopInput tele_input(teleop_vesc[0], teleop_vesc[1], teleop_vesc[2]);

  // Velocity Profile
  float amax = 3.0;	// m/s^2
  static float vout_x, vout_y, vout_z;
  TrapezoidalVelProfile v_prof_x(amax, 1./rate_hz);
  TrapezoidalVelProfile v_prof_y(1.0*amax, 1./rate_hz);
  TrapezoidalVelProfile v_prof_z(4.*amax, 1./rate_hz);

  // ROS Loop
  uint32_t cnt_lp = 0;
  ros::Rate loop_rate(rate_hz); //Hz
  ROS_INFO("Start Tele-operation");
  teleop_vesc[0]->enable.data = true;
  teleop_vesc[0]->startTime = ros::Time::now();
  //teleop_vesc2->startTime = ros::Time::now();
  //teleop_vesc3->startTime = ros::Time::now();

  // set VESC can id here
  teleop_vesc[0]->controller_id[0] = TARGET_VESC_ID_LOCAL;
  teleop_vesc[0]->controller_id[1] = 101;

  bool send_param_flag = false;
  bool send_data_flag = false;

  while (ros::ok())
  { 
		// read encoder data.
		//teleop_vesc1->requestCustoms();
		//ROS_INFO("rps_0:%.2f(dps_0:%.2f), rad_0:%.2f", teleop_vesc1->rps[0], teleop_vesc1->rps[0]*RPS2DPS, teleop_vesc1->rad[0]);
		//ROS_INFO("rps_1:%.2f(dps_1:%.2f), rad_1:%.2f", teleop_vesc1->rps[1], teleop_vesc1->rps[1]*RPS2DPS, teleop_vesc1->rad[1]);
		//ROS_INFO("target_dps_0:%.2f, dps_0:%.2f, deg_0:%.2f", value_goto, teleop_vesc1->rps[0]*RPS2DPS, teleop_vesc1->rad[0]*RAD2DEG);
		//ROS_INFO("current_0:%.2f, dps_0:%.2f, deg_0:%.2f", value_goto, teleop_vesc1->rps[0]*RPS2DPS, teleop_vesc1->rad[0]*RAD2DEG);
		//ROS_INFO("target_dps_1:%.2f, dps_1:%.2f, deg_1:%.2f", -value_goto, teleop_vesc1->rps[1]*RPS2DPS, teleop_vesc1->rad[1]*RAD2DEG);

/*
		// 5A(2min), 10A(1min) Cycle use
		// CAN Master Devs is directly connected to USB and it's ID should be set as TARGET_VESC_ID
		teleop_vesc[0]->custom_cmd_type[0] = COMM_SET_CURRENT;//COMM_SET_RELEASE;COMM_SET_DPS;COMM_SET_DUTY;//COMM_SET_GOTO;
		if(cnt>=0) 			teleop_vesc[0]->custom_cmd_value[0] = 5.;
		if(cnt>=(50*120))	teleop_vesc[0]->custom_cmd_value[0] = 10.0;
		if(cnt>=(50*180))	cnt = 0;
		cnt++;
	*/

		//teleop_vesc[0]->controller_id[1] = 25;
		//teleop_vesc[0]->custom_cmd_type[1] = COMM_SET_CURRENT;//COMM_SET_CURRENT;//COMM_SET_OR_GOTO;
		//teleop_vesc[0]->custom_cmd_value[1] = 0.;//0.5;

		// set initial VMAX value here
		teleop_vesc[0]->custom_cmd_type[0] = COMM_SET_DPS_VMAX;
		teleop_vesc[0]->custom_cmd_value[0] = 1000;
		teleop_vesc[0]->custom_cmd_type[1] = COMM_SET_DPS_VMAX;
		teleop_vesc[0]->custom_cmd_value[1] = 1000;
		teleop_vesc[0]->setCustomOut();

		teleop_vesc[0]->custom_cmd_type[0] = COMM_SET_RELEASE;//COMM_SET_RELEASE;COMM_SET_DPS;COMM_SET_DUTY;//COMM_SET_SERVO;//COMM_SET_TRAJ
		teleop_vesc[0]->custom_cmd_value[0] = 0;	//100dps
		teleop_vesc[0]->custom_cmd_type[1] = COMM_SET_DPS;
		teleop_vesc[0]->custom_cmd_value[1] = 0;	//2000dps
		if(teleop_vesc[0]->enable.data == true) teleop_vesc[0]->setCustomOut();

		//ROS_INFO("rps_0:%.2f(dps_0:%.2f), rad_0:%.2f", teleop_vesc[0]->rps[0], teleop_vesc[0]->rps[0]*RPS2DPS, teleop_vesc[0]->rad[0]);
		//ROS_INFO("rps_1:%.2f(dps_1:%.2f), rad_1:%.2f", teleop_vesc[0]->rps[1], teleop_vesc[0]->rps[1]*RPS2DPS, teleop_vesc[0]->rad[1]);

		ros::spinOnce();
		loop_rate.sleep();
  }

  return 0;
}
