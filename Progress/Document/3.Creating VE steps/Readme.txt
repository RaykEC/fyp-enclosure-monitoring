To Setup Backend Venv testing environment.

# create a folder\path to the folder that you wish to store, then change directory to the folder.
mkdir C:\Users\Admin\FYP_Backend
cd C:\Users\Admin\FYP_Backend

#create virtual environment (venv) file name "venv" in the folder that you wish to store and execute in.
python -m venv venv

#Activate the virtual environment.
venv\Scripts\activate

#if successfully activated, (venv) will be include in the command prompt:
(venv) C:\Users\Admin\FYP_Backend

#install all packages via pip - fastapi, uvicorn, paho-mqtt, psycopg2-binary.
pip install fastapi uvicorn paho-mqtt psycopg2-binary

---------------------------------------------------------------------------------------------------------------------

#create a new notepad file at directory folder with venv folder (example: C:\Users\Admin\FYP_Backend), rename saved notepad as mqtt_subscriber.py and copy the python code into in then save.
