# Manual Whisper Model Installation

To use the AI Lyrics Transcription feature offline or via manual download, follow these steps:

## 1. Download the Model
We recommend the **Small** model for the best balance of speed and accuracy on your GTX 1060.

| Model | Size | Download Link |
| :--- | :--- | :--- |
| **Small** (Recommended) | ~461 MB | [Download small.pt](https://openaipublic.azureedge.net/main/whisper/models/9ecf779972d90ba49f06944ce279eeb9/small.pt) |
| Base | ~139 MB | [Download base.pt](https://openaipublic.azureedge.net/main/whisper/models/ed3a0b6b1c0edf879ad9b11b1af5a0e6/base.pt) |
| Medium | ~1.42 GB | [Download medium.pt](https://openaipublic.azureedge.net/main/whisper/models/345ae4da6259e58e2b55531eb3d6a97c/medium.pt) |

## 2. Place the File
1.  Go to your project folder: `D:\Document\NC-KTV`
2.  Create a folder named `models` if it doesn't exist.
3.  Inside `models`, create a folder named `whisper`.
4.  **Copy the downloaded `.pt` file into:** `D:\Document\NC-KTV\models\whisper\`

**Final Path Example:**
`D:\Document\NC-KTV\models\whisper\small.pt`

## 3. Application Setup
 The application is already configured to check this `models/whisper` folder. If it finds the file, it will use it directly without attempting to download anything.
