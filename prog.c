#include <stdint.h>
#include <Pipes.h>

inline float fpmul32f(float x, float y)
{
	return(x*y);
}

inline float fpadd32f(float x, float y)
{
	return(x+y);
}

inline float fpsub32f(float x, float y)
{
	return(x-y);
}


inline uint32_t fpadd32fi(uint32_t x, uint32_t y)
{
	return(x+y);
}

inline uint32_t fpsub32fi(uint32_t x, uint32_t y)
{
	return(x-y);
}


inline uint32_t udiv32(uint32_t dividend, uint32_t divisor) 
{
   uint32_t remainder = 0;
   uint32_t quotient = 0xffffffff;
   remainder = 0;
   if(divisor == 0)
   {
	return(0xffffffff);
   }
   else if(divisor == 1)
   {
	return(dividend);
   }
   else if(divisor > dividend)
   {
	quotient = 0;
	remainder = dividend;
	return(quotient);
   }
   else
   {
	quotient = 0;
	while(dividend >= divisor)
	{
		uint32_t curr_quotient = 1;
       		uint32_t dividend_by_2 = (dividend >> 1);
		uint32_t shifted_divisor = divisor;

		while(1)
		{
			if(shifted_divisor < dividend_by_2)
			{
				shifted_divisor = (shifted_divisor << 1);
				curr_quotient = (curr_quotient << 1);
			}
			else	
		  		break;
		}

			quotient = fpadd32fi(quotient,curr_quotient);
			dividend = fpsub32fi(dividend,shifted_divisor);
	}

	remainder = dividend;
   }
   return(quotient);
}


inline float fdiv32(float a, float b)
{
	uint32_t mantissa_a = 0, mantissa_b = 0;
	uint32_t exponent_a = 0, exponent_b = 0;
	uint32_t sign_a = 0, sign_b = 0;
	uint32_t sign = 0;
	uint32_t exp = 0;
	uint32_t man = 0;
	uint32_t ival_a = 0, ival_b = 0;
	uint32_t temp = 0;
	float out_div = 0;

	if (a==0)
		return(0);
	else{
		ival_a = *((uint32_t *)&a);
		ival_b = *((uint32_t *)&b);


		exponent_a = ((ival_a & (0x7F800000))>>23);
		exponent_b = ((ival_b & (0x7F800000))>>23);

		mantissa_a = ((ival_a & (0x007FFFFF)) | (0x00800000))<<7;
		mantissa_b = ((ival_b & (0x007FFFFF)) | (0x00800000))>>7;

		sign_a = (ival_a & (0x80000000))>>31;
		sign_b = (ival_b & (0x80000000))>>31;

		sign = (sign_a ^ sign_b)<<31;
		exp = fpsub32fi(exponent_a, exponent_b);
		man = (udiv32(mantissa_a,mantissa_b));

		temp = man;
		while( ( (temp & (0x00800000)) != 0x00800000) && (temp !=0) )
		{
		man = (man << 1);
		exp = fpsub32fi(exp, 1);
		temp = man;
		}

		man = ((man)& (0x007FFFFF));
		exp = fpadd32fi(exp,136) <<23; // 128 + 7 + 8 -3

		temp = (sign | exp | man);

		out_div = *((float *)&temp); 
	}
	return(out_div);
}

inline float rotor_flux_calc(float del_t, float Lm, float id, float flux_rotor_prev, float tau_new, float tau_r){

	double temp_a = 0, temp_b = 0, temp_c = 0;
	double temp_flux_n = 0,temp_flux_d = 0;
	float flux_rotor = 0;

	temp_a = fpmul32f(50000.0,Lm);
	temp_b = fpmul32f(id,temp_a); 
	temp_c = fpmul32f(103919.753,flux_rotor_prev);

	temp_flux_n = fpadd32f(temp_c,temp_b);

	flux_rotor = fdiv32(temp_flux_n,103969.753);
	
	return(flux_rotor);
}
inline float omega_calc(float Lm, float iq, float tau_r, float flux_rotor){
	float temp_omega_n = 0,temp_omega_d = 0;
	float omega_r = 0;
	temp_omega_n = fpmul32f(Lm,iq);
	temp_omega_d = fpmul32f(tau_r,flux_rotor);
	omega_r = fdiv32(temp_omega_n,temp_omega_d);
	return(omega_r);
}

float theta_calc(float omega_r, float omega_m, float del_t, float theta_prev){
	float temp_a = 0, temp_b = 0;
	float theta = 0;
	temp_a = fpmul32f(omega_r,omega_m);
	temp_b = fpmul32f(temp_a,del_t);
	theta = fpadd32f(theta_prev,temp_a);
	return(theta);
}

inline float iq_err_calc(float Lr, float torque_ref, float constant_1, float flux_rotor){

	float temp_d = 0;
	float temp_iq_n = 0,temp_iq_d = 0;
	float iq_err = 0;

	if (flux_rotor<1)
		flux_rotor = 1;
	else flux_rotor = flux_rotor;
	
	temp_d = fpmul32f(2000000.0,Lr);
	temp_iq_n = fpmul32f(temp_d,torque_ref);
	temp_iq_d = fpmul32f(constant_1,flux_rotor);

	iq_err = fdiv32(temp_iq_n,temp_iq_d);
	return(iq_err);
}

void vector_control_daemon(){

	float id = 0; float iq = 0; float torque_ref = 0; float flux_ref = 0; float speed = 0; float speed_ref = 0;
	float torque_sat_high = 30, torque_sat_low = -30;
	float speed_err = 0, int_speed_err = 0, prop_speed_err = 0;
	float flux_err = 0, int_flux_err = 0, prop_flux_err = 0, flux_add = 0;
	float Kp = 40, Ki = 50;
	float Kp_n = 40000000, Ki_n = 50000000;
	float Lm = 0.8096;
	float Lm_n = 809600;
	float Lr = 0.84175;
	float tau_r = 0.103919753;
	float flux_rotor = 0;
	float flux_rotor_prev = 0;
	float del_t = 50e-6;
	float del_t_new = 50e-9;
	float flux_ref_prev = 0;
	float tau_new = 0;
	float theta = 0;
	float theta_prev = 0;
	float omega_r = 0;
	float omega_m = 314.1592654;
	float id_err = 0;
	float iq_err = 0;
	float poles = 4;
	float nf = 300;
	float constant_temp = fpmul32f(3000,poles);
	float constant_1 = fpmul32f(constant_temp,Lm);
	float int_speed_err_temp_1 = 0;
	float int_speed_err_temp_2 = 0;
	float int_flux_err_temp_1 = 0;
	float int_flux_err_temp_2 = 0;
	float flux_ref_calc_temp_1 = 0;
	float flux_ref_calc_temp_2 = 0;

	
	while(1){
	
		//Read Data from motor
		id  = read_float32("in_data1");
		iq  = read_float32("in_data2");
		speed  = read_float32("in_data3");
		speed_ref  = read_float32("in_data4");	
		
		//Generation of Reference Values
		speed_err = fpsub32f(speed_ref,speed);
		//Torque Reference Value Calculations
		int_speed_err_temp_1 = fpmul32f(del_t,speed_err);
		int_speed_err_temp_2 = fpadd32f(int_speed_err_temp_1,int_speed_err);
		int_speed_err = fpmul32f(Ki,int_speed_err_temp_2); 
		if (int_speed_err < -15.0)
			int_speed_err = -15.0;
		else if (int_speed_err > 15.0)
			int_speed_err = 15.0;
		else
			int_speed_err = int_speed_err;
	
		prop_speed_err = fpmul32f(speed_err,Kp);
	
		torque_ref = fpadd32f(int_speed_err,prop_speed_err);
		
		
		if (torque_ref < torque_sat_low)
			torque_ref = torque_sat_low;
		else if (torque_ref > torque_sat_high)
			torque_ref = torque_sat_high;
		else
			torque_ref = torque_ref;
		
		//Flux Reference Value Calculations
		if (speed_ref <=2000.0)
			flux_ref = nf;
		else if (speed_ref <=2500.0){
			flux_ref_calc_temp_1 = fpmul32f(-0.0002,speed_ref);
			flux_ref_calc_temp_2 = fpadd32f( flux_ref_calc_temp_1, 1.4); 
			flux_ref = fpmul32f(flux_ref_calc_temp_2 ,nf);
		}
		else if (speed_ref <=3000.0){
			flux_ref_calc_temp_1 = fpmul32f(-0.00036,speed_ref);
			flux_ref_calc_temp_2 = fpadd32f( flux_ref_calc_temp_1, 1.8); 
			flux_ref = fpmul32f(flux_ref_calc_temp_2 ,nf);
		}
		else{ 
			flux_ref_calc_temp_1 = fpmul32f(-0.00042,speed_ref);
			flux_ref_calc_temp_2 = fpadd32f( flux_ref_calc_temp_1, 1.98); 
			flux_ref = fpmul32f(flux_ref_calc_temp_2 ,nf);
		}
		
		//Vector Control Begins Here
		tau_new= fpadd32f(del_t,tau_r);
		
		flux_rotor =  rotor_flux_calc( del_t,  Lm,  id,  flux_rotor_prev,  tau_new, tau_r);
		
		omega_r =  omega_calc( Lm,  iq,  tau_r,  flux_rotor);
		theta =  theta_calc( omega_r,  omega_m,  del_t,  theta_prev);

		iq_err = iq_err_calc( Lr,  torque_ref,  constant_1,  flux_rotor);
		
		//iD Calculations
		flux_err = fpsub32f(flux_ref,flux_rotor);
		int_flux_err_temp_1 = fpmul32f(del_t_new,flux_err);
		int_flux_err_temp_2 = fpadd32f(int_flux_err_temp_1,int_flux_err);
		int_flux_err = fpmul32f(Ki_n,int_flux_err_temp_2); 		
		if (int_flux_err < -1000000)
			int_flux_err = -1000000;
		else if (int_flux_err > 1000000)
			int_flux_err = 1000000;
		else
			int_flux_err = int_flux_err;
		
		prop_flux_err = fpmul32f(flux_err,Kp_n);
		
		flux_add = fpadd32f(int_flux_err,prop_flux_err);
		
		if (flux_add < -2000000)
			flux_add = -2000000;
		else if (flux_add > 2000000)
			flux_add = 2000000;
		else
			flux_add = flux_add;
		
		id_err = fdiv32(flux_add,Lm_n);

		
		flux_ref_prev = flux_ref;

		flux_rotor_prev = flux_rotor;
		theta_prev = theta;
		
		//Write Back Generated Data
		write_float32("out_data1",id_err);
		write_float32("out_data2",iq_err);
		write_float32("out_data3",omega_r);
		write_float32("out_data4",flux_rotor);
	}
}
