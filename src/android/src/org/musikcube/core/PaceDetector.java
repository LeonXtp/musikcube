package org.musikcube.core;

import java.util.Collections;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;


public class PaceDetector implements Runnable, SensorEventListener{
	
	static final public float MAX_BPM	= 100;
	static final public float MIN_BPM	= 40;
	static final public int WAVE_MEMORY	= 20;
	static final public int WAVE_MIN_CALC	= 10;
	static final public float WAVE_MIN_BPM_DIFF = 100;	// This is in miliseconds
	static final public int WAVE_COMPARE	= 4;
	static final public int MIN_PLAYTIME	= 20000;	// Play at leased 20 seconds of a track
	//static final public float BPM_THRESHOLD	= 10;	// if BPM is off by more than 10 bpm, switch track
	
	private float currentBPM	= 0;
	private float currentAccuracy = 0;
	private long currentBPMstart	= 0;
	private Object lock	= new Object();
	
	private Context context;
	
	private class PaceDimension{
		public java.util.ArrayList<Long> beatTimes	= new java.util.ArrayList<Long>(); 
		public java.util.ArrayList<Float> amplitude	= new java.util.ArrayList<Float>(); 
		
		private float lastValue	= 0;
		private float lastDiff	= 0;
		private float currentMax	= 0;
		private float currentMin	= 0;
		
		public float currentBPM			= 0;
		public float currentAccuracy	= 0;
		
		final public void NextValue(float value){
			final float diff	= value-this.lastValue;

			if(value<this.currentMin){
				this.currentMin	= value;
			}
			if(value>this.currentMax){
				this.currentMax	= value;
			}
			
			if(this.lastDiff>=0 && diff<0){
				
				// Amplitude must be at leased 5
				if(this.currentMax-this.currentMin<4){
					this.currentMin	= value;
					this.currentMax	= value;
				}else{
					// this is a top on the curve
					this.beatTimes.add(android.os.SystemClock.elapsedRealtime());
					this.amplitude.add(this.currentMax-this.currentMin);
	
					// Reset the amplitude
					this.currentMin	= value;
					this.currentMax	= value;
					// only keep the last 10 waves
					while(this.beatTimes.size()>WAVE_MEMORY){
						this.beatTimes.remove(0);
						this.amplitude.remove(0);
					}
	
					// Lets calculate BPM
					long bpmSum	= 0;
					//java.util.TreeSet<Long> bpms	= new java.util.TreeSet<Long>();
					java.util.ArrayList<Long> bpms	= new java.util.ArrayList<Long>();
					for(int i=0;i<this.beatTimes.size()-WAVE_COMPARE;i++){
						long orgSample	= this.beatTimes.get(i);
						for(int j=i+1;j<i+WAVE_COMPARE;j++){
							//float bpmSample	= 60000/(this.beatTimes.get(j)-orgSample);
							long bpmSample	= this.beatTimes.get(j)-orgSample;
							//Log.v("MC2::C","s "+bpmSample);
							if(bpmSample>(60000/MAX_BPM) && bpmSample<(60000/MIN_BPM)){
								bpms.add(bpmSample);
								bpmSum	+= bpmSample;
							}
						}
					}
					Collections.sort(bpms);
					//Log.v("MC2::BEAT","B "+(bpms.size()));
					
					// Lets remove the most "off" samples and correct the AVG until we are down to the desired "diff"
					boolean qualified	= false;
					long bpmDiff	= 0;
	
					while(!qualified && bpms.size()>=WAVE_MIN_CALC){
						Long first	= bpms.get(0);
						Long last	= bpms.get(bpms.size()-1);
						bpmDiff	= last-first;
						int bpmSize	= bpms.size();
	
	//	Log.v("MC2::DIFF","diff "+bpmSize+" "+first+"-"+last+" diff="+bpmDiff);
						
						if(bpmDiff<WAVE_MIN_BPM_DIFF){
							qualified	= true;
						}else{
							// Remove the element that is most far away from the average
							long avg	= bpmSum/bpmSize;
							if(avg-first>last-avg){
								// Remove first
								bpmSum	-= first;
								bpms.remove(0);
							}else{
								// Remove last 
								bpmSum	-= last;
								bpms.remove(bpms.size()-1);
							}
						}
					}
					
					if(qualified){
						// Get avg amplitude
						float amplitude	= this.amplitude.get(0);
						for(int i=1;i<this.amplitude.size();i++){
							amplitude+=this.amplitude.get(i);
						}
						amplitude	/= this.amplitude.size();
						
						
						this.currentBPM	= ((float)60000*bpms.size())/((float)bpmSum);
						this.currentAccuracy	= (100-bpmDiff)+bpms.size()*13+amplitude*5;
						PaceDetector.this.ChangeBPM(this.currentBPM,this.currentAccuracy);
					}
				}
			}
			this.lastDiff	= diff;
			this.lastValue	= value;
		}
		
	}
	
	public interface OnBPMListener{
		public void OnBPMUpdate();
	}
	protected OnBPMListener listener			= null;
	
	public void SetListener(OnBPMListener listener){
		synchronized(this.lock){
			this.listener	= listener;
			if(this.listener!=null){
				this.listener.OnBPMUpdate();
			}
		}
	}
	
	
	private PaceDimension xAxis	= new PaceDimension(); 
	private PaceDimension yAxis	= new PaceDimension(); 
	private PaceDimension zAxis	= new PaceDimension(); 
	
	public void onAccuracyChanged(Sensor sensor, int accuracy) {
		
	}

	public void onSensorChanged(SensorEvent event) {
		float values[]	=	event.values; 
		this.xAxis.NextValue(values[0]);
		this.yAxis.NextValue(values[1]);
		this.zAxis.NextValue(values[2]);
	}
	
	public void ChangeBPM(float bpm,float accuracy){
		while(bpm<85){
			bpm	*= 2;
		}
		while(bpm>200){
			bpm	*= 0.5;
		}
		
		if(accuracy>=this.xAxis.currentAccuracy && accuracy>=this.yAxis.currentAccuracy && accuracy>=this.zAxis.currentAccuracy && accuracy>150){
			// The BPM has changed
			
			long currentTime	= android.os.SystemClock.elapsedRealtime();
//			if(currentTime>this.currentBPMstart+MIN_PLAYTIME){
				//Log.v("BPM","3 "+bpm);
				// We have played more than minimum time
				
//				if(bpm>this.currentBPM+BPM_THRESHOLD || bpm<this.currentBPM-BPM_THRESHOLD){
					// BPM has changed enough to switch track
					this.currentBPMstart	= currentTime; 

					synchronized(lock){
						this.currentBPM	= bpm;
						if(this.listener!=null){
							this.listener.OnBPMUpdate();
						}
					}
					this.currentAccuracy	= accuracy;
					
//				}
//			}
		}
	}
	
	public void StartSensor(Context context){
		this.context	= context;
		SensorManager sensorMgr	= (SensorManager)context.getSystemService(Context.SENSOR_SERVICE);
		Sensor accelerometer	= sensorMgr.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
		sensorMgr.registerListener(this,accelerometer,SensorManager.SENSOR_DELAY_GAME);
	}
	
	public void StopSensor(Context context){
		SensorManager sensorMgr	= (SensorManager)context.getSystemService(Context.SENSOR_SERVICE);
		sensorMgr.unregisterListener(this);
	}
	
	

	public void run() {
	}
	
	public float GetBPM(){
		synchronized(this.lock){
			return currentBPM;
		}
	}

}
