# integration.roon

This integration hub is intended for control of **ROON servers** with the **YIO remote**. (https://github.com/YIO-Remote).
It is port of Roonlabs Javascript API (https://github.com/RoonLabs) to Qt/C++. 
The QtRoon* part is also usable for other Qt/ROON integrations.

Discovery is not available until now, the port and IP address of the server must be configured. 
Important: the entities friendly_name must be the name of the ROON instance :

Entity:
>"media_player": [
>      {
>          "entity_id": "media_player.work",
>          "friendly_name": "Arbeitsplatz",
>          "integration": "roon",
>          "type:": "speaker",
>           "supported_features": 

Integration :
>{
>           "url": "ws://192.168.1.100:9100/api",
>            "imageurl": "http://192.168.1.100:9100/api/image/",
>            "friendly_name": "RIC's Roon Control",
>            "id": "roon",
>            "log": "info"
>       } ]
       
YioRoon.* communicates with they YIO remote software. I made some (small) extensions to the media-player entity :

* Adding a property **browseItems**. It is a list of media entry objects containing  
    o	Item_key	key of the entry  
    o	title  
    o	sub_title  
    o	image_url	for albums, and artists, (accessing the ROON server)  
		
* Adding a property **browseCmds**. Up to 3 commands which are currently use for 3 buttons on the head of the list.  
  Currently I work only with simulation maybe on the real YIO we need only the hardware buttons.  
  This commands are necessary as ROON provides several possible commands on the different hierarchy levels :  
    o	Artist		play tracks from this artists randomly  
    o	Album		play whole album immediately, add to queue  
    o	Track		play now, next, queue  
		
* Adding a function **browse (cmd)**  
  cmd is any of the commands from browseCmds (TOP, BACK, PLAY, QUEUE) or the selected enries item_key.


