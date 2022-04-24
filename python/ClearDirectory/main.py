import os

def ClearDir(directory):
    directory = directory.replace('\\','/').replace('\n', '') + '/' if directory[-1] != '/' else directory.replace('\\','/')
    print(f'SANDBOX --> Clearing {directory}')
    directoryFiles = os.listdir(directory)
    for file in directoryFiles:
        os.remove(directory + file)
    print(f'SANDBOX --> Successfully Cleared {directory}')

if __name__ == '__main__':
    directoryList = open('directorylist.txt', 'r').readlines()
    for directory in directoryList:
        ClearDir(directory)
