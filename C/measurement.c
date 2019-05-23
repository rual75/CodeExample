
#include "measurement.h"
#include "sensor/bme280.h"

 
/// список типов ЭХД
const ECS_TYPE esc_type_list[ESC_TYPE_NUMBER]  = {
    
    /// датчик хлора 0-20 ppm
    {
        "MEMBRAPOR Cl2/M-20",               
    
        {   /// AFE config
            LMP91000_TIA_GAIN_350K,  LMP91000_RLOAD_33_Ohm,

            LMP91000_REF_EXTERNAL,  LMP91000_ZERO_50PS,

            LMP91000_BIAS_NEG, LMP91000_BIAS_0PS,

            LMP91000_FET_NO_SHORT , LMP91000_MODE_3LEAD  
        }, 

        // range
        0.0f, 20.0f,

        /// temperature
        -20.0f, +45.0f,

        ///  pressure 1013 mbar +- 10% от нормы
        911.925f, 1114.575f, 

        /// humidity 
        15.0f, 90.0f,

        /// наработка 
        (365UL * 2 * 24 * 60 * 60),  /// 2 года в секундах
    },
                                                      
};


bool measure_tick;

uint32_t ESC_work_count;

bool meteo_init(void)
{
    dbase.meteo_ID = BME280_init();
    
    return (dbase.meteo_ID == 0x58) || (dbase.meteo_ID == 0x60);
}

/// состояния КА измерения ЭХД
typedef  enum {
    S_ESC_INIT = 0,
    S_ESC_WAIT,           
    S_ESC_EXPOSITION,               
    S_ESC_TEMP_MSR,
    S_ESC_CONC_MSR,    
    S_ESC_SLEEP,  
    S_ESC_CONC_UNLIM_MSR, 
    S_ESC_TEMP_UNLIM_MSR       
}ESC_MSR_STATE;    


/// состояние КА измерений ЭХД
static ESC_MSR_STATE esc_state = S_ESC_SLEEP;
static CALIB_VECTOR meteo_vec;

void ESC_measure(void)
{    
    static uint32_t time;
    static uint32_t counter;
    static int32_t data;
    static int32_t ADC_AFE_offset = 0;
       
    /// проверка корректности типа датчика
    if (dbase.ecs_type >= ESC_TYPE_NUMBER) dbase.ecs_type = 0;
    
    /// переход к калибровке 
    if (IsECS_Calibration() && (esc_state != S_ESC_EXPOSITION)
            && (esc_state != S_ESC_CONC_MSR)
            && (esc_state != S_ESC_TEMP_MSR))
    {
        counter = 0;
        esc_state = S_ESC_WAIT; 
    }
 
    debug_base.ESC_state = esc_state;
    
    switch (esc_state)
    {
        case S_ESC_INIT: /// инициализация AFE
            if (LMP91000_Config((LMP91000_CONFIG_Struct*)&esc_type_list[dbase.ecs_type].AFE_config)) 
            {
                //LMP91000_Mode(esc_type_list[dbase.ecs_type].AFE_config.Mode  /// переключение на выход TIA
                //        /*| LMP91000_FET_SHORT*/);      /// с закороченным входом 
                
                data = 0;
                counter = dbase.ESC_wait_sec;
                esc_state = S_ESC_WAIT;                 
            }
            else /// AFE не отвечает
                if (millis() - time > 2000) 
                {
                    dbase.flags |= (1 << MSR_ERR_AFE);                     
                    esc_state = S_ESC_SLEEP; 
                }                                
            break;
            
            
        case S_ESC_WAIT: /// пауза между измерения 
            
            if (dbase.ESC_wait_sec == 0) 
            {
                counter = dbase.ESC_exp_sec;
                time = millis();
                esc_state = S_ESC_CONC_UNLIM_MSR; 
               
            } else
            
            if (counter-- == 0)  
            {
                CoollerOn(); 
                counter = dbase.ESC_exp_sec;
                
                debug_base.ADC_offset = 
                        ADC_AFE_offset = data / dbase.ESC_wait_sec; 
                             
                time = millis();
                esc_state = S_ESC_EXPOSITION;                               
            }
                        
            else data += ADC_read();
            
            break;     
            
            
        case S_ESC_EXPOSITION:    /// экспозиция        
            if (counter-- == 0)  
            {            
                CoollerOff(); 
                
                counter = dbase.ESC_temp_sec; // время измерения температуры                             
                
                data = 0;                 
                LMP91000_Mode(LMP91000_MODE_TEMP_TIA_ON); /// переключение на датчик температуры AFE                                
                
                time = millis();
                esc_state = S_ESC_TEMP_MSR;                                
            }             
            else if ((millis() - time > 2000) 
                    && (dbase.cool_rpm < COOLLER_MIN_RPM)) dbase.flags |= (1 << MSR_ERR_COOL); /// ошибка вентилятора
            break;              
            
       
        
        case S_ESC_TEMP_MSR: /// измерение температуры    
            
            if (dbase.cool_rpm > 0) {
                if (millis() - time > 2000) dbase.flags |= (1 << MSR_ERR_COOL); /// ошибка вентилятора        
                else break;                
            }             
            
            if (counter-- == 0)  
            {               
                counter = dbase.ESC_concentration_sec; /// время измерения тока ЭХД
                
                /// вычисление температуры AFE
                dbase.Temperature_AFE = LMP91000_TempCalc((K_ADC * 1000) * 
                        (data / dbase.ESC_temp_sec));
                
                LMP91000_Mode(esc_type_list[dbase.ecs_type].AFE_config.Mode); /// переключение на выход TIA                                    
                                
                data = 0;
                esc_state = S_ESC_CONC_MSR;   
            }  
            
            else data += ADC_read();
            
            break;   
            
         case S_ESC_CONC_MSR: /// измерение концентрации         
            if (counter-- == 0)  
            {  
                 if (IsECS_Calibration()) /// завершим калибровку
                {
                    dbase.Concentration = 
                        ECS_ConcentrationCalc(data / dbase.ESC_concentration_sec );
                     
                    ECS_CalibrationEnd();
                    
                    meteo_vec.delta = dbase.Calibration - dbase.Concentration;
                    
                    AddCalibration(&meteo_vec);
                }      
                /// вычисление концентрации
                else dbase.Concentration = CalibCorrection(
                    ECS_ConcentrationCalc(data / dbase.ESC_concentration_sec ));               
                
                 
                esc_state = S_ESC_SLEEP;                                             
            }       
            
            else 
            {                
                data += ADC_read();
                float delta = (esc_type_list[dbase.ecs_type].range_max - esc_type_list[dbase.ecs_type].range_min) /10;
                if ((dbase.Concentration > esc_type_list[dbase.ecs_type].range_max + delta) 
                            || (dbase.Concentration < esc_type_list[dbase.ecs_type].range_min - delta)) 
                    dbase.flags |= (1 << MSR_ERR_ECS);            
            }
            break;
            
            
        case S_ESC_CONC_UNLIM_MSR: /// длительное измерение концентрации         
            
            CoollerOn();
            
            if (millis() - time > 2000) { /// ошибка вентилятора
                    if (dbase.cool_rpm < COOLLER_MIN_RPM) dbase.flags |= (1 << MSR_ERR_COOL); 
                    else dbase.flags &= ~(1 << MSR_ERR_COOL);
            }            
           
            /// вычисление концентрации             
            dbase.Concentration = CalibCorrection(ECS_ConcentrationCalc(ADC_read()));   
            
            LMP91000_Mode(LMP91000_MODE_TEMP_TIA_ON); /// переключение на датчик температуры AFE                                
                
            float delta = (esc_type_list[dbase.ecs_type].range_max - esc_type_list[dbase.ecs_type].range_min) /10;
            if ((dbase.Concentration > esc_type_list[dbase.ecs_type].range_max + delta) 
                        || (dbase.Concentration < esc_type_list[dbase.ecs_type].range_min - delta)) 
                dbase.flags |= (1 << MSR_ERR_ECS);           
            
            esc_state = S_ESC_TEMP_UNLIM_MSR;            
            break;
        
            
        case S_ESC_TEMP_UNLIM_MSR: /// измерение температуры AFE                
            /// вычисление температуры 
            dbase.Temperature_AFE = LMP91000_TempCalc((K_ADC * 1000) * ADC_read()); 

            if (dbase.ESC_wait_sec) /// завершим длительное измерение
            {
                CoollerOff();
                esc_state = S_ESC_SLEEP;  
            }
            else 
            {
                LMP91000_Mode(esc_type_list[dbase.ecs_type].AFE_config.Mode); /// переключение на выход TIA                                    

                esc_state = S_ESC_CONC_UNLIM_MSR;    
                ErrorRenew(); 
            }                
            break;   
        
        case S_ESC_SLEEP:
        default: 
            ECS_CalibrationEnd();
            ErrorRenew();
            time = millis();
            LMP91000_Sleep();            
            esc_state = S_ESC_INIT; 
            break;
    }
}


/// общий цикл измерений 
void measure_run(void)
{             
    if (dbase.ADC_Status = ADC_run()) dbase.flags |= (1 << MSR_ERR_ADC);  
    dbase.ADC_result = (float)ADC_read() * K_ADC; 
    
    debug_base.ADC_result = ADC_read();   
    
    
    if(measure_tick) /// ежесекундный вызов обработчика ЭХД
    {       
       ESC_measure();
       
       dbase.ECS_work_count = ESC_work_count += 1;    
       
       if (dbase.control_pass == 0x02040604UL) 
        {
            ESC_work_count = 0;
            dbase.control_pass = 0;
        }

        else if (dbase.control_pass == 0x01030007UL) /// установка часов            
        {
            dbase.rtc.rtc.control_reg = dbase.rtc.rtc.year;
            dbase.rtc.rtc.year = dbase.rtc.rtc.month;
            dbase.rtc.rtc.month = dbase.rtc.rtc.date;
            dbase.rtc.rtc.date = dbase.rtc.rtc.day;
            dbase.rtc.rtc.day = dbase.rtc.rtc.hour;
            dbase.rtc.rtc.hour = dbase.rtc.rtc.minutes;            
            dbase.rtc.rtc.seconds = 0; /// адрес
            dbase.rtc.rtc.minutes = 0; /// секунды            
            
            RTC_Write(dbase.rtc.buff);
            dbase.control_pass = 0;
        }       
        
       else if (dbase.control_pass == 0) 
       {
            RTC_Read(dbase.rtc.buff);         
       }   
       
       write_ESC_work_count( ESC_work_count);
      
       measure_tick = false;        
    } 
    
    else 
    
    {
        static uint8_t msr_stat = 0;
        static uint16_t cycle = 0;

        if (cycle++ > 1000) /// цикл расчетов метео
        {         
            cycle = 0;  

            switch (msr_stat++)
            {            
                case 0: 
                    meteo_vec.temperature 
                            = dbase.Temperature =  BME280_readTempC();
                    break;

                case 1: 
                    meteo_vec.pressure 
                            = dbase.Pressure = BME280_readFloatPressure()/100.0f;
                    break;    

                case 2:
                    meteo_vec.humidity
                            = dbase.Humidity = BME280_readFloatHumidity();
                    break;

                case 3:
                    dbase.meteo_ID =  BME280_GetID();
                    CalibRenew();
                    break;

                default:
                    if (msr_stat > 2000); 
                        msr_stat = 0;
                    break;
            }

        } else { /// подбор окружения     
            ///dbase.flags = 
            CalibSelector(&meteo_vec);            
        }
    }
}
