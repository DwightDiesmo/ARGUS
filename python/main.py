import os
import time
import numpy
import soundfile
from datetime import datetime
from PIL import Image
import pandas
import pynmea2 as nmea
import matplotlib.pyplot as plt
import mysql.connector
from ftplib import FTP

ftpDirectoryPath = 'C:/Users/dwigh/Desktop/FTP/'
ftpFiles = os.listdir(ftpDirectoryPath)

def getFTP():
    ftp = FTP('192.168.43.132')
    ftp.login('portentah7', 'argus2')
    ftpDir = '/argus0'
    ftp.cwd(ftpDir)
    files = ftp.nlst()
    print(files)
    os.chdir(ftpDirectoryPath)
    for fileName in files:
        if os.path.isfile(fileName):
            print('Downloading')
            ftp.retrbinary("RETR %s" %fileName, open(fileName, 'wb').write)
    ftp.close()
    # print(ftp.dir())

def clearDir(directory):
    directory = directory.replace('\\','/').replace('\n', '') + '/' if directory[-1] != '/' else directory.replace('\\','/')
    print(f'ARGUS II --> Clearing {directory}')
    directoryFiles = os.listdir(directory)
    for file in directoryFiles:
        os.remove(directory + file)
    print(f'ARGUS II --> Successfully Cleared {directory}')

def formatFiles(directory, rawFile):
    with open(directory + rawFile) as rawFP:
        moduleDataLine = rawFP.readline()
        formattedDataFile = open('formatted/' + rawFile, 'w')
        while moduleDataLine:
            formatedData = moduleDataLine.replace('&', '\n&\n')
            formattedDataFile.write(formatedData)
            moduleDataLine = rawFP.readline()
    rawFP.close()

def parseFiles(readDirectory, writeDirectory):
    readDirectoryFiles = os.listdir(readDirectory)
    for readFile in readDirectoryFiles:
        currentFile = open(readDirectory + readFile, 'r').readlines()
        audioFile = open(writeDirectory + 'audio/' + readFile, 'w')
        audioFile.write(currentFile[2].replace(' ', '\n').strip())
        audioFile.close()
        seismicFile = open(writeDirectory + 'seismic/' + readFile, 'w')
        seismicFile.write(currentFile[4].replace('_', '\n').strip())
        seismicFile.close()
        imageFile = open(writeDirectory + 'image/' + readFile, 'w')
        imageFile.write(currentFile[6].replace('_', '\n').strip())
        imageFile.close()
        gpsFile = open(writeDirectory + 'gps/' + readFile, 'w')
        for i in range(8, 14):
            gpsFile.write(currentFile[i].replace('_',''))
        gpsFile.close()

def processAudio(rawDirectory, duration, targetDirectory, id):
    rawFiles = os.listdir(rawDirectory)
    for file in rawFiles:
        print(file)
        rawData = open(rawDirectory + file, 'r').readlines()
        length = len(rawData)
        sampleRate = length/duration
        buffer = [0 for i in range(length)]
        scaled = [0 for i in range(length)]
        for i in range(0,length):
            scaled[i] = float(int(rawData[i]) * 2)/1023
        scaled = numpy.asarray(scaled)
        file.replace('.txt','')
        soundfile.write(targetDirectory + '/' + id + '.wav', scaled, int(sampleRate))

def processImage(rawDirectory, targetDirectory, id):
    rawFiles = os.listdir(rawDirectory)
    for file in rawFiles:
        rawData = open(rawDirectory + file, 'r').readlines()
        buffer = [0 for i in range(len(rawData))]
        for i in range(1,len(rawData)-1):
            buffer[i] = int(rawData[i])
        byteData = bytes(buffer)
        imgSize = (320,320)
        img = Image.frombytes('L', imgSize, byteData)
        img.save(targetDirectory + id + '.jpg')

def processSeismic(rawDirectory, targetDirectory, id):
    rawFiles = os.listdir(rawDirectory)
    idCounter = '0'
    for file in rawFiles:
        rawData = pandas.read_csv(rawDirectory + file, delimiter=' ')
        rawData.to_csv('processed/seismic/csv/' + file.replace('.txt', '.csv'), index=None)
        counter = 0
        for col in rawData.columns:
            counter += 1
            plt.figure()
            plt.plot(rawData[col])
            column = 'x' if counter == 1 else 'y' if counter == 2 else 'z'
            plt.savefig(targetDirectory + column + '-' + str(idCounter) + '.png')
        idCounter = str(int(idCounter) + 1)

def processGPS(rawDirectory, targetDirectory, id):
    rawFiles = os.listdir(rawDirectory)
    idCounter = '0'
    for file in rawFiles:
        rawData = open(rawDirectory + file, 'r').readlines()
        for line in rawData:
            if '$GPGGA' in line:
                msg = nmea.parse(line)
                # print(msg)
                gpsFile = open(targetDirectory + idCounter + '.txt', 'w')
                gpsFile.write(f'{msg.latitude}\n')
                gpsFile.write(f'{msg.longitude}\n')
                gpsFile.write(f'{msg.timestamp}')
                gpsFile.close()
        idCounter = str(int(idCounter) + 1)

def createID():
    connection = mysql.connector.connect(host="localhost", port="3306", user="root", database="argus")
    cursor = connection.cursor()
    selectQuery = "SELECT id FROM id"
    cursor.execute(selectQuery)
    listID = cursor.fetchall()
    lastID = 0
    for id in listID:
        print(id)
        integerID = int(id[0])
        if integerID > lastID:
            lastID = integerID
    print(f'Last ID {lastID}')
    insertQuery = "INSERT INTO `id` (`id`) VALUES (%s);"
    if lastID == None:
        newID = '0'
    else:
        newID = str(lastID + 1)
        # print(newID)
    insertData = (newID,)
    cursor.execute(insertQuery, insertData)
    cursor.execute("COMMIT;")
    cursor.close()
    connection.close()
    return newID

def main():
    oldFTPFiles = ftpFiles
    while(1):
        newFTPFiles = os.listdir(ftpDirectoryPath)
        # processImage('parsed/image/', 'processed/image/')
        if(oldFTPFiles != newFTPFiles):
            print('ARGUS II --> FTP Updated ' + datetime.now().strftime("%d/%m/%Y %H:%M:%S %p"))
            time.sleep(5)
            for rawFile in newFTPFiles:
                formatFiles(ftpDirectoryPath, rawFile)
            print('ARGUS II --> DATA REFORMATTED ' + datetime.now().strftime("%d/%m/%Y %H:%M:%S %p"))
            parseFiles('formatted/', 'parsed/')
            # time.sleep(15)
            print('ARGUS II --> DATA PARSED ' + datetime.now().strftime("%d/%m/%Y %H:%M:%S %p"))
            newID = createID()
            print(f'New File ID: {newID}')
            processAudio('parsed/audio/', 60, 'C:/Users/dwigh/PycharmProjects/ArgusWebApp/static/data/audio/', newID)
            print('ARGUS II --> AUDIO PROCESSING COMPLETE ' + datetime.now().strftime("%d/%m/%Y %H:%M:%S %p"))
            processImage('parsed/image/', 'C:/Users/dwigh/PycharmProjects/ArgusWebApp/static/data/image/', newID)
            print('ARGUS II --> IMAGE PROCESSING COMPLETE ' + datetime.now().strftime("%d/%m/%Y %H:%M:%S %p"))
            processSeismic('parsed/seismic/', 'C:/Users/dwigh/PycharmProjects/ArgusWebApp/static/data/seismic/', newID)
            print('ARGUS II --> SEISMIC PROCESSING COMPLETE ' + datetime.now().strftime("%d/%m/%Y %H:%M:%S %p"))
            processGPS('parsed/gps/', 'C:/Users/dwigh/PycharmProjects/ArgusWebApp/static/data/gps/', newID)
            print('ARGUS II --> GPS PROCESSING COMPLETE ' + datetime.now().strftime("%d/%m/%Y %H:%M:%S %p"))
        oldFTPFiles = newFTPFiles


if __name__ == "__main__":
    main()