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

class LowPassFilter:
    xk_1=0
    yk_1=0
    gain = 1
    def __init__(self,fc,fm):
        beta=math.exp((-2*math.pi * fc) / fm)
        self.param1=1-beta
        self.param2=beta
    def filter_in(self,in_k_1,out_k_1):
        return self.param1*in_k_1 + self.param2*out_k_1

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

    def setxkk_1(self,xvalue):
        self.xk_1=xvalue

    def setxyk_1(self,yvalue):
        self.yk_1=yvalue  

    def update(self,xk_1,yk_1):
        self.setxkk_1(xk_1)
        self.setxyk_1(yk_1)

    def printParam(self):
        print(self.param1)
        print(self.param2)
    

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

def searchMaxFrequency(signal):
        yf=fft(signal)
        yf_abs= (255/32768)*2.0/CHUNK * np.abs(yf[0:CHUNK//2]) #array 1024 length
        index=np.argmax(yf_abs)
        return index
def limitmaxValue(input,limit_sup):
    if input>limit_sup : input=limit_sup
    return input
def limitminValue(input,limit_inf):
    if input<limit_inf : input=0
    return input

def offsetValue(input,offset):
    return input+offset

def correctNegativeColor(color):
    if color <0: color = 100 +color
    return color

def setBrightness(freq):
    if freq< 1.5: brightness=0
    else: brightness=freq*(80/100) +20
    return brightness

def setColorfromFreq(freq):
    temp_color=offsetValue(freq,-3)   
    temp_color=correctNegativeColor(temp_color)
    color_rgb=hvs2rgb(temp_color/100)
    return color_rgb
def AdjustGain(value,multiply):
    return value*multiply


class Player:
    def __init__(self):
        self.root=Tk()
        self.root.title("Control Led Strip")
        self.root.config(bg="grey")
        #self.root.geometry("800x400")

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
        #Radiobutton(self.select_mode_canvas,text="Rainbow led",variable=self.VarAudioColor,value=4).grid(row=1,column=4) 

        

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

        self.FilterLeft= LowPassFilter(500,self.sample_rate)
        self.FilterRight= LowPassFilter(500,self.sample_rate)
        self.FilterMono=LowPassFilter(500,self.sample_rate)

        self.FilterBrightness=LowPassFilter(1,self.sample_rate/CHUNK)
        self.FilterBrightness_Left=LowPassFilter(1,self.sample_rate/CHUNK)
        self.FilterBrightness_Right=LowPassFilter(1,self.sample_rate/CHUNK)
        

        self.FilterFreq=LowPassFilter(0.1,self.sample_rate/(CHUNK//2))
        self.FilterFreq_Left=LowPassFilter(0.1,self.sample_rate/(CHUNK//2))
        self.FilterFreq_Right=LowPassFilter(0.1,self.sample_rate/(CHUNK//2))


    
        

        clock=1

        try:
            while self.playing:
                data = stream.read(CHUNK)
                signal_array= np.frombuffer(data,dtype=np.int16)
                
                signal_left=signal_array[::2]  #split data from stream read in two channels   
                signal_right=signal_array[1::2]         
                
                if self.VarAudioMode.get()==1:
                    signal_mono=(signal_left+signal_right)/2

                    #filter audio
                    self.FilterMono.setgain(self.gain_Strip_led.get())
                    signal_mono_filtered=self.FilterMono.filter_array(signal_mono)
                    signal_mono_filtered=np.multiply(signal_mono_filtered,RESOLUTUION_ESP32)

                    #filter brightness
                    brightness=max(signal_mono_filtered)
                    brightness_filtered=self.FilterBrightness.filter_in(self.FilterBrightness.xk_1,self.FilterBrightness.yk_1)
                    self.FilterBrightness.update(brightness,brightness_filtered)

                    brightness_left=int(brightness_filtered)
                    brightness_right=int(brightness_filtered)

                elif self.VarAudioMode.get()==2:
                    #filter audio left
                    self.FilterLeft.setgain(self.gain_Strip_led.get())    
                    signal_left_filtered=self.FilterLeft.filter_array(signal_left)
                    signal_left_filtered=np.multiply(signal_left_filtered,RESOLUTUION_ESP32)

                    #filter audio right
                    self.FilterRight.setgain(self.gain_Strip_led.get())
                    signal_right_filtered=self.FilterRight.filter_array(signal_right)
                    signal_right_filtered=np.multiply(signal_right_filtered,RESOLUTUION_ESP32)

                    #filter brightness left channel
                    brightness_left=max(signal_left_filtered)           
                    brightness_left_filtered=self.FilterBrightness_Left.filter_in(self.FilterBrightness_Left.xk_1,self.FilterBrightness_Left.yk_1)
                    self.FilterBrightness_Left.update(brightness_left,brightness_left_filtered)
                    
                    #filter brightness right channel
                    brightness_right=max(signal_right_filtered)
                    brightness_right_filtered=self.FilterBrightness_Right.filter_in(self.FilterBrightness_Right.xk_1,self.FilterBrightness_Right.yk_1)
                    self.FilterBrightness_Right.update(brightness_right,brightness_right_filtered)

                    brightness_left=int(brightness_left_filtered)
                    brightness_right=int(brightness_right_filtered)      

                else:
                    self.playing=False
                    break
                    
                #limit to 255 for send
                brightness_left=limitmaxValue(brightness_left,255)
                brightness_right=limitmaxValue(brightness_right,255)
            
                if self.VarAudioColor.get()==1: #Color select
                    if self.VarAudioMode.get()==1:
                        color_left=self.color_mono
                        color_right=self.color_mono
                    elif self.VarAudioMode.get()==2:
                        color_left=self.color_left
                        color_right=self.color_right

                elif self.VarAudioColor.get()==2: #Rainbown
                    clock=clock+1
                    clock_devided=frecuency_devider(clock,10)
                    color_rainbow=rainbow_color(clock_devided)
                    color_left=color_rainbow
                    color_right= color_rainbow
                    
                elif self.VarAudioColor.get()==3: # response Frecuency
                   
                    if self.VarAudioMode.get()==1:
                        #Analysis and filter signal
                        max_freq=searchMaxFrequency(signal_mono)
                        max_freq_filtered=self.FilterFreq.filter_in(self.FilterFreq.xk_1,self.FilterFreq.yk_1)
                        self.FilterFreq.update(max_freq,max_freq_filtered)

                        max_freq_filtered=limitmaxValue(max_freq_filtered,100)

                        #color set
                        color_dft=setColorfromFreq(max_freq_filtered)   
                        color_left=color_dft
                        color_right=color_dft

                        
                    elif self.VarAudioMode.get()==2:
                        
                        #Analysis and filter left signal
                        max_freq_left=searchMaxFrequency(signal_left)
                        max_freq_left_filtered=self.FilterFreq_Left.filter_in(self.FilterFreq_Left.xk_1,self.FilterFreq_Left.yk_1)
                        self.FilterFreq_Left.update(max_freq_left,max_freq_left_filtered)


                        #Analysis and filter right signal
                        max_freq_right=searchMaxFrequency(signal_right)
                        max_freq_right_filtered=self.FilterFreq_Right.filter_in(self.FilterFreq_Right.xk_1,self.FilterFreq_Right.yk_1)
                        self.FilterFreq_Right.update(max_freq_right,max_freq_right_filtered)
                        

                        max_freq_left_filtered=limitmaxValue(max_freq_left_filtered,100)
                        max_freq_right_filtered=limitmaxValue(max_freq_right_filtered,100)
                        
                        color_left=setColorfromFreq(max_freq_left_filtered)
                        color_right=setColorfromFreq(max_freq_left_filtered)


                                    
                messageUDP =  color_left+str(brightness_left)+'-'+color_right+str(brightness_right)
                print("UDP send: ",messageUDP)
                
                sock.sendto(bytes(messageUDP, "utf-8"), (self.Ip_text.get(), UDP_PORT))
        finally:
            print("Thread end")
        
           
    def stop_task(self):
        self.playing = False
        msg_off="#Connect_Off"
        sock.sendto(bytes(msg_off, "utf-8"), (self.Ip_text.get(), UDP_PORT))
        

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
