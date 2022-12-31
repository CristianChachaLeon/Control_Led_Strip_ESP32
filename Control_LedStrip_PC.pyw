from tkinter import *
from tkinter.colorchooser import askcolor
import socket
import pyaudio
import threading
import time
import numpy as np
import colorsys
from scipy.fft import fft, fftfreq
import matplotlib.pyplot as plt
import math

import os

#Configuration record output
CHANNELS=2
FORMAT = pyaudio.paInt16
CHUNK=2048
RESOLUTUION_ESP32=255/32767

#UDP socket 
UDP_PORT=8888
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # UDP

#---------UDP connect
def sendConnectUDP(ip,port):
    sock.sendto(bytes("#Connect_On", "utf-8"), (ip, port))
    print("Conect send")


#filter class

    #Gain for filter
GAIN_AUIDO=5   #cambiar por potenciometro

class FilterLowPass:
    xk_1=0
    yk_1=0
    gain = 1
    def filter_in(self,in_k_1,out_k_1):
        return 0.006524*in_k_1+0.9935*out_k_1
    
    def amplifier(self,data_array):
        return np.multiply(data_array,self.gain)

    def filter_array(self,data_array):
        array_filtered=[]
        for data in data_array:
            y=self.filter_in(self.xk_1,self.yk_1)
            array_filtered.append(y)
            self.yk_1=y
            self.xk_1=data
        return self.amplifier(array_filtered)
    def setgain(self,input_gain):
        self.gain=input_gain

class FilterLowPass2:
    xk_1=0
    yk_1=0
    gain = 1
    def filter_in(self,in_k_1,out_k_1):
        return 0.02645*in_k_1+0.9735*out_k_1



def frecuency_devider(input,devider):
    return input//devider 

def rainbow_color(index):
    DIVISION_RAINBOWN =50
    position= index- frecuency_devider(index,DIVISION_RAINBOWN)*DIVISION_RAINBOWN
    (r, g, b) = colorsys.hsv_to_rgb(position/DIVISION_RAINBOWN, 1, 1)
    color="#{:02x}{:02x}{:02x}".format(int(r*255),int(g*255),int(b*255)).upper()
    return color
def hvs2rgb(hue):
    (r, g, b) = colorsys.hsv_to_rgb(hue, 1, 1)
    color="#{:02x}{:02x}{:02x}".format(int(r*255),int(g*255),int(b*255)).upper()
    return color

class Player:
    def __init__(self):
        self.root=Tk()
        self.root.title("Control Led Strip")
        self.root.config(bg="grey")
        self.root.geometry("800x400")

        self.playing=False
        self.selected_input=BooleanVar()
        self.gain_Strip_led=DoubleVar()
        self.VarAudioMode=IntVar()
        self.VarAudioMode.set(1)
        self.VarAudioColor=IntVar()
        self.VarAudioColor.set(1)

        self.mode=1

        self.color_mono="#FF0000"
        self.color_left="#FF0000"
        self.color_right="#FF0000"
        
        self.Display=Frame(self.root,width=1200,height=600)
        self.Display.grid()

        self.Ip_label=Label(self.Display,text="IP ESP32:",padx=10,pady=10)
        self.Ip_label.grid(row=0,column=0)

        self.Ip_text = Entry(self.Display,justify="right")
        self.Ip_text.grid(row=0,column=1, padx=10)
        self.Connect_button = Button(self.Display,text="Connect",command=lambda:sendConnectUDP(self.Ip_text.get(),UDP_PORT))
        self.Connect_button.grid(row=0,column=2)
        

        self.device_button=Button(self.Display,text="Show Device Input",command=self.init_task)
        self.device_button.grid(row=0,column=3,padx=50)

        self.Stop_button=Button(self.Display,text="Stop",command=self.stop_task)
        self.Stop_button.grid(row=0,column=4)

        self.canvas=Canvas(self.Display)
        self.canvas.grid(row=2,column=0,columnspan=3)
        self.scrollbar=Scrollbar(self.canvas,orient=VERTICAL)
        self.scrollbar.pack(side=RIGHT, fill=Y)

        self.printscreen=Listbox(self.canvas,width=70)
        self.printscreen.pack()
        self.printscreen.config(yscrollcommand=self.scrollbar.set)
        self.scrollbar.config(command=self.printscreen.yview)

        self.Select_button=Button(self.Display,text="select",command=self.select_input)
        self.Select_button.grid(row=2,column=3)

        self.gain_selector=Scale(self.Display,variable=self.gain_Strip_led,from_=10,to=0,orient=VERTICAL)
        self.gain_selector.grid(row=2,column=4)
        Label(self.Display,text="Gain").grid(row=3,column=4)

        self.select_mode_canvas=Canvas(self.Display)
        self.select_mode_canvas.grid(row=1,column=0,columnspan=2)
        Radiobutton(self.select_mode_canvas,text="Mono",variable=self.VarAudioMode,value=1).grid(row=0,column=0)
        Radiobutton(self.select_mode_canvas,text="Stereo",variable=self.VarAudioMode,value=2).grid(row=1,column=0) 

        
        self.color_mono_button=Button(self.select_mode_canvas,width=5,command=self.change_color)
        self.color_mono_button.grid(row=0,column=1)
        
        
        
        self.color_left_button=Button(self.select_mode_canvas,width=5,command=self.change_color_left)
        self.color_left_button.grid(row=1,column=1)
        self.color_right_button=Button(self.select_mode_canvas,width=5,command=self.change_color_right)
        self.color_right_button.grid(row=1,column=2)
        
        Radiobutton(self.select_mode_canvas,text="Select Color",variable=self.VarAudioColor,value=1).grid(row=0,column=3)
        Radiobutton(self.select_mode_canvas,text="Rainbow strip",variable=self.VarAudioColor,value=2).grid(row=1,column=3) 
        Radiobutton(self.select_mode_canvas,text="Response Frecuency",variable=self.VarAudioColor,value=3).grid(row=0,column=4)
        Radiobutton(self.select_mode_canvas,text="Rainbow led",variable=self.VarAudioColor,value=4).grid(row=1,column=4) 

        

        self.root.mainloop()
        

    def init_task(self):
        t=threading.Thread(target=self.music)
        t.start()

    def music(self):
        self.playing = True
        self.p=pyaudio.PyAudio()

        self.show_list_device()    
        
        self.Select_button.wait_variable(self.selected_input)

        self.audio_input = self.printscreen.curselection()[0]
        self.printscreen.delete(0,END)
        self.selected_input.set(False)
        self.canvas.pack_forget()
        self.sample_rate=int(self.p.get_device_info_by_index(self.audio_input).get('defaultSampleRate'))

        stream = self.p.open(
            input_device_index=int(self.audio_input),
            format=FORMAT,
            channels=CHANNELS,
            rate=self.sample_rate,
            input=True,
            frames_per_buffer=CHUNK
        )

        self.FilterLeft= FilterLowPass()
        self.FilterRight= FilterLowPass()
        self.FilterMono=FilterLowPass()
        self.FilterFreq=FilterLowPass2()

        fig, ax = plt.subplots()

        frecuency=fftfreq(CHUNK,1/self.sample_rate)[:CHUNK//2]
        list_split=[3,6,9,12,20,30,50,70,150,200,250,347,524,1024]
        table_hue=[0,0.06,0.09,0.11,0.20,0.35,0.5,0.6,0.7,0.8,0.9,0.95,0.99,1]
        freq_selec=np.ones(1024)
        freq_selec_filtered=np.ones(1024)
        print(len(freq_selec))
        
        line, =ax.plot(range(1024),np.random.rand(1024))
        line2 , =ax.plot(range(1024),np.random.rand(1024))

        ax.set_ylim(0,100)
        ax.set_xlim(0,1024)
        #ax.set_yscale('log')
        #ax.set_xscale('log')
        clock=1

        filter_1=np.ones(684)
        filter_0=np.ones(340)*0.1
            
        filter_freq=np.concatenate((filter_0,filter_1))

        
        while self.playing:
            data = stream.read(CHUNK)
            signal_array= np.frombuffer(data,dtype=np.int16)
            
            signal_left=signal_array[::2]  #split data from stream read in two channels
            signal_right=signal_array[1::2]         

            if self.VarAudioMode.get()==1:
                signal_mono=(signal_left+signal_right)/2
                self.FilterMono.setgain(self.gain_Strip_led.get())
                signal_mono_filtered=self.FilterMono.filter_array(signal_mono)
                signal_mono_fitlered=np.multiply(signal_mono_filtered,RESOLUTUION_ESP32)

                brightness=int(max(signal_mono_fitlered))
                brightness_left=brightness
                brightness_right=brightness
                #color="#FF00AA"
                if brightness>255:
                    brightness=255
                

            elif self.VarAudioMode.get()==2:
                self.FilterLeft.setgain(self.gain_Strip_led.get())
                self.FilterRight.setgain(self.gain_Strip_led.get())
                signal_left_filtered=self.FilterLeft.filter_array(signal_left)
                signal_right_filtered=self.FilterRight.filter_array(signal_right)

                signal_left_filtered=np.multiply(signal_left_filtered,RESOLUTUION_ESP32)
                signal_right_filtered=np.multiply(signal_right_filtered,RESOLUTUION_ESP32)

                brightness_left=int(max(signal_left_filtered))
                brightness_right=int(max(signal_right_filtered))

                    #limit to 255 for send
                if brightness_left>255:
                    brightness_left=255
                if brightness_right>255:
                    brightness_right=255
                      
            else:
                self.playing=False
                break
          
            if self.VarAudioColor.get()==1: #Color select
                if self.VarAudioMode.get()==1:
                    color_left=self.color_mono
                    color_right=self.color_mono
                elif self.VarAudioMode.get()==2:
                    color_left=self.color_left
                    color_right=self.color_right

                #self.mode = 1
                
                
            elif self.VarAudioColor.get()==2: #Rainbown
                clock=clock+1
                clock_devided=frecuency_devider(clock,10)
                color=rainbow_color(clock_devided)
                color_left=color
                color_right= color
                #self.color_mono_button.grid_remove()
            elif self.VarAudioColor.get()==3: # response Frecuency
                frecuency=fftfreq(CHUNK,1/self.sample_rate)[:CHUNK//2]
                GAIN_SPECTRE=1024/930
                if self.VarAudioMode.get()==1:
                    
                    yf=fft(signal_mono)
                    
                    yf_abs= (255/32768)*2.0/CHUNK * np.abs(yf[0:CHUNK//2]) #array 1024 length

                    index_max_value=np.argmax(yf_abs)

                    index_max_value_filter=self.FilterFreq.filter_in(self.FilterFreq.xk_1,self.FilterFreq.yk_1)
                    self.FilterFreq.xk_1=index_max_value
                    self.FilterFreq.yk_1=index_max_value_filter

                    

                    freq_selec=np.append(freq_selec,index_max_value)
                    freq_selec=freq_selec[1:]

                    freq_selec_filtered=np.append(freq_selec_filtered,index_max_value_filter)
                    freq_selec_filtered=freq_selec_filtered[1:]

                   
                    line.set_ydata(freq_selec)
                    line2.set_ydata(freq_selec_filtered)
                    
                    fig.canvas.draw()
                    fig.canvas.flush_events()
                    plt.pause(0.001)
                    

                    
                    if index_max_value_filter >100 : index_max_value_filter=100


                    index_color=index_max_value_filter - 3
                    if index_color <0: 
                        index_color = 100 +index_color
                    color_dft=hvs2rgb(index_color   /100)
                                       
                    color_left=color_right=color_dft
                    if index_max_value_filter< 1.5:
                        brightness=0
                    else:
                        brightness=index_max_value_filter*(80/100) +20
                    brightness_left=int(brightness * (255/100)*(self.gain_Strip_led.get()/10))
                    brightness_right=int(brightness * (255/100)*(self.gain_Strip_led.get()/10))
                    
                elif self.VarAudioMode.get()==2:
                    yf_left=fft(signal_left)
                    yf_right=fft(signal_right)
                    
                    yf_abs_left=np.abs(yf_left[0:CHUNK//2])
                    i_max_left=np.argmax(yf_abs_left)
                    color_dft_left=rainbow_color(int(i_max_left*GAIN_SPECTRE* (50/1024)))
                    color_left=color_dft_left
                    
                    yf_abs_right=np.abs(yf_right[0:CHUNK//2])
                    i_max_right=np.argmax(yf_abs_right)
                    color_dft_right=rainbow_color(int(i_max_right*GAIN_SPECTRE* (50/1024)))
                    color_right=color_dft_right

                                
            messageUDP =  color_left+str(brightness_left)+'-'+color_right+str(brightness_right)
            print("UDP send: ",messageUDP)
            
            sock.sendto(bytes(messageUDP, "utf-8"), (self.Ip_text.get(), UDP_PORT))
        
           
    def stop_task(self):
        self.playing = False

    def select_input(self):
       self.selected_input.set(True)
       
       print("boton pulsado")

    def show_list_device(self):
        for i in range(0,self.p.get_device_count()):
                    if  self.p.get_device_info_by_index(i).get('maxInputChannels')>0:
                        m="input device id: " + str(i) +"-" +self.p.get_device_info_by_index(i).get('name')
                        self.printscreen.insert(i,m)
    def change_color(self):
        colors = askcolor(title="Tkinter Color Chooser")
        if colors[1] is not None:
            self.color_mono=colors[1].upper()
            self.color_mono_button.config(background=self.color_mono)
    def change_color_left(self):
        colors = askcolor(title="Tkinter Color Chooser")
        if colors[1] is not None:
            self.color_left=colors[1].upper() 
            self.color_left_button.config(background=self.color_left)  
    def change_color_right(self):
        colors = askcolor(title="Tkinter Color Chooser")
        if colors[1] is not None:
            self.color_right=colors[1].upper()
            self.color_right_button.config(background=self.color_right)  



if __name__=="__main__":
    Player()
