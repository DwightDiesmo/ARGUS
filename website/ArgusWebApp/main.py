from flask import Flask, render_template, request, flash, url_for, redirect, session
import mysql.connector

app = Flask(__name__)

@app.route("/")
def home():
    connection = mysql.connector.connect(host="localhost", port="3306", user="root", database="argus")
    cursor = connection.cursor()
    selectQuery = "SELECT * from id"
    cursor.execute(selectQuery)
    activities = cursor.fetchall()
    maxID = int(activities[-1][0])
    print(maxID)
    gpsData = []
    for activity in activities:
        # print(activity[0])
        gpsFile = open(f'static/data/gps/{activity[0]}.txt', 'r')
        latitude = gpsFile.readline()
        longitude = gpsFile.readline()
        time = gpsFile.readline()
        currentData = ((latitude, longitude, time))
        gpsData.append(currentData)
    print(gpsData)
    return render_template("index.html", activities=activities, gpsData=gpsData, maxID=maxID)

@app.route("/<activityID>/")
def activityPage(activityID):
    connection = mysql.connector.connect(host="localhost", port="3306", user="root", database="argus")
    cursor = connection.cursor()
    selectQuery = f"SELECT * FROM id where id.id={activityID}"
    cursor.execute(selectQuery)
    activityData = cursor.fetchone()
    gpsFile = open(f'static/data/gps/{activityID}.txt', 'r')
    latitude = gpsFile.readline()
    longitude = gpsFile.readline()
    time = gpsFile.readline()
    return render_template("activity-page.html", activityData=activityData, latitude=latitude, longitude=longitude, time=time)

if __name__=="__main__":
    app.run(debug=True)