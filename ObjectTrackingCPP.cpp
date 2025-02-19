// ObjectTrackingCPP.cpp

#include<opencv2/core/core.hpp>
#include<opencv2/highgui/highgui.hpp>
#include<opencv2/imgproc/imgproc.hpp>

#include<iostream>
#include<conio.h>           // it may be necessary to change or remove this line if not using Windows

#include "Blob.h"

#define SHOW_STEPS            // un-comment or comment this line to show steps or not

// global variables ///////////////////////////////////////////////////////////////////////////////
const cv::Scalar SCALAR_BLACK = cv::Scalar(0.0, 0.0, 0.0);
const cv::Scalar SCALAR_WHITE = cv::Scalar(255.0, 255.0, 255.0);
const cv::Scalar SCALAR_YELLOW = cv::Scalar(0.0, 255.0, 255.0);
const cv::Scalar SCALAR_GREEN = cv::Scalar(0.0, 255.0, 0.0);
const cv::Scalar SCALAR_RED = cv::Scalar(0.0, 0.0, 255.0);

// function prototypes ////////////////////////////////////////////////////////////////////////////
void matchCurrentFrameBlobsToExistingBlobs(std::vector<Blob> &existingBlobs, std::vector<Blob> &currentFrameBlobs);
void addBlobToExistingBlobs(Blob &currentFrameBlob, std::vector<Blob> &existingBlobs, int &intIndex);
void addNewBlob(Blob &currentFrameBlob, std::vector<Blob> &existingBlobs);
double distanceBetweenBlobs(Blob firstBlob, Blob secondBlob);
void drawBlobInfoOnImage(std::vector<Blob> &blobs, cv::Mat &imgFrame2Copy);

///////////////////////////////////////////////////////////////////////////////////////////////////
int main(void) {

    cv::VideoCapture capVideo;

    cv::Mat imgFrame1;
    cv::Mat imgFrame2;

    std::vector<Blob> blobs;
    
    capVideo.open("CarsDrivingUnderBridge.mp4");

    if (!capVideo.isOpened()) {                                                 // if unable to open video file
        std::cout << "error reading video file" << std::endl << std::endl;      // show error message
        _getch();                    // it may be necessary to change or remove this line if not using Windows
        return(0);                                                              // and exit program
    }

    if (capVideo.get(CV_CAP_PROP_FRAME_COUNT) < 2) {
        std::cout << "error: video file must have at least two frames";
        _getch();
        return(0);
    }

    capVideo.read(imgFrame1);
    capVideo.read(imgFrame2);

    char chCheckForEscKey = 0;

    bool blnFirstFrame = true;

    while (capVideo.isOpened() && chCheckForEscKey != 27) {

        std::vector<Blob> currentFrameBlobs;

        cv::Mat imgFrame1Copy = imgFrame1.clone();
        cv::Mat imgFrame2Copy = imgFrame2.clone();

        cv::Mat imgDifference;
        cv::Mat imgThresh;

        cv::cvtColor(imgFrame1Copy, imgFrame1Copy, CV_BGR2GRAY);
        cv::cvtColor(imgFrame2Copy, imgFrame2Copy, CV_BGR2GRAY);

        cv::GaussianBlur(imgFrame1Copy, imgFrame1Copy, cv::Size(5, 5), 0);
        cv::GaussianBlur(imgFrame2Copy, imgFrame2Copy, cv::Size(5, 5), 0);

        cv::absdiff(imgFrame1Copy, imgFrame2Copy, imgDifference);

        cv::threshold(imgDifference, imgThresh, 30, 255.0, CV_THRESH_BINARY);

        cv::imshow("imgThresh", imgThresh);

        cv::Mat structuringElement3x3 = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
        cv::Mat structuringElement5x5 = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
        cv::Mat structuringElement7x7 = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(7, 7));
        cv::Mat structuringElement15x15 = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(15, 15));

        for (unsigned int i = 0; i < 2; i++) {
            cv::dilate(imgThresh, imgThresh, structuringElement5x5);
            cv::dilate(imgThresh, imgThresh, structuringElement5x5);
            cv::erode(imgThresh, imgThresh, structuringElement5x5);
        }

        cv::Mat imgThreshCopy = imgThresh.clone();

        std::vector<std::vector<cv::Point> > contours;

        cv::findContours(imgThreshCopy, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        for (auto &contour : contours) {
            Blob possibleBlob(contour);

            if (possibleBlob.boundingRect.area() > 500 &&
                possibleBlob.dblAspectRatio > 0.25 &&
                possibleBlob.dblAspectRatio < 4.0 &&
                possibleBlob.boundingRect.width > 30 &&
                possibleBlob.boundingRect.height > 30 &&
                possibleBlob.dblDiagonalSize > 60.0) {
                currentFrameBlobs.push_back(possibleBlob);
            }
        }

        if (blnFirstFrame == true) {
            for (auto &currentFrameBlob : currentFrameBlobs) {
                blobs.push_back(currentFrameBlob);
            }
        } else {
            matchCurrentFrameBlobsToExistingBlobs(blobs, currentFrameBlobs);
        }

        cv::Mat imgContours(imgThresh.size(), CV_8UC3, SCALAR_BLACK);
        contours.clear();

        for (auto &blob : blobs) {
            if (blob.blnStillBeingTracked == true) {
                contours.push_back(blob.contour);
            }
        }

        cv::drawContours(imgContours, contours, -1, SCALAR_WHITE, -1);

        cv::imshow("imgContours", imgContours);

        imgFrame2Copy = imgFrame2.clone();          // get another copy of frame 2 since we changed the previous frame 2 copy in the processing above

        drawBlobInfoOnImage(blobs, imgFrame2Copy);

        cv::imshow("imgFrame2Copy", imgFrame2Copy);

                    // now we prepare for the next iteration

        currentFrameBlobs.clear();

        imgFrame1 = imgFrame2.clone();           // move frame 1 up to where frame 2 is

        if ((capVideo.get(CV_CAP_PROP_POS_FRAMES) + 1) < capVideo.get(CV_CAP_PROP_FRAME_COUNT)) {
            capVideo.read(imgFrame2);
        } else {
            std::cout << "end of video\n";
            break;
        }

        blnFirstFrame = false;

        chCheckForEscKey = cv::waitKey(1);
    }

    if (chCheckForEscKey != 27) {               // if the user did not press esc (i.e. we reached the end of the video)
        cv::waitKey(0);                         // hold the windows open to allow the "end of video" message to show
    }
        // note that if the user did press esc, we don't need to hold the windows open, we can simply let the program end which will close the windows

    return(0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void matchCurrentFrameBlobsToExistingBlobs(std::vector<Blob> &existingBlobs, std::vector<Blob> &currentFrameBlobs) {
    
    for (auto &existingBlob : existingBlobs) {
        existingBlob.blnCurrentMatchFoundOrNewBlob = false;

        int intNumPtsSoFar = existingBlob.actualCenterPositions.size();

        cv::Mat matPredicted = existingBlob.kalmanFilter.predict();

        cv::Point ptPredicted((int)matPredicted.at<float>(0), (int)matPredicted.at<float>(1));

        cv::Mat matActualMousePosition(2, 1, CV_32F, cv::Scalar::all(0));

        matActualMousePosition.at<float>(0, 0) = (float)existingBlob.actualCenterPositions[intNumPtsSoFar - 1].x;
        matActualMousePosition.at<float>(1, 0) = (float)existingBlob.actualCenterPositions[intNumPtsSoFar - 1].y;

        cv::Mat matCorrected = existingBlob.kalmanFilter.correct(matActualMousePosition);        // function correct() updates the predicted state from the measurement

        cv::Point ptCorrected((int)matCorrected.at<float>(0), (int)matCorrected.at<float>(1));

        existingBlob.predictedCenterPositions.push_back(ptPredicted);
        existingBlob.correctedCenterPositions.push_back(ptCorrected);
    }

    for (auto &currentFrameBlob : currentFrameBlobs) {

        int intIndexOfLeastDistance = 0;
        double dblLeastDistance = 1000000.0;

        for (unsigned int i = 0; i < existingBlobs.size() - 1; i++) {
            if (existingBlobs[i].blnStillBeingTracked == true) {
                double dblDistance = distanceBetweenBlobs(currentFrameBlob, existingBlobs[i]);

                if (dblDistance < dblLeastDistance) {
                    dblLeastDistance = dblDistance;
                    intIndexOfLeastDistance = i;
                }
            }
        }

        if (dblLeastDistance < currentFrameBlob.dblDiagonalSize * 2.0) {                // !!!!!!! left off here, change to use prediction rather than distance !!!
            addBlobToExistingBlobs(currentFrameBlob, existingBlobs, intIndexOfLeastDistance);
        } else {
            addNewBlob(currentFrameBlob, existingBlobs);
        }

    }

    for (auto &existingBlob : existingBlobs) {
        if (existingBlob.blnCurrentMatchFoundOrNewBlob == false) {
            existingBlob.blnStillBeingTracked = false;
        }
    }

}

///////////////////////////////////////////////////////////////////////////////////////////////////
void addBlobToExistingBlobs(Blob &currentFrameBlob, std::vector<Blob> &existingBlobs, int &intIndex) {
    
    existingBlobs[intIndex].contour = currentFrameBlob.contour;
    existingBlobs[intIndex].boundingRect = currentFrameBlob.boundingRect;

    existingBlobs[intIndex].ptCurrentCenter = currentFrameBlob.ptCurrentCenter;

    existingBlobs[intIndex].actualCenterPositions.push_back(existingBlobs[intIndex].ptCurrentCenter);

    existingBlobs[intIndex].dblDiagonalSize = currentFrameBlob.dblDiagonalSize;
    existingBlobs[intIndex].dblAspectRatio = currentFrameBlob.dblAspectRatio;
    
    //existingBlobs[intIndex].vectorOfAllActualPoints.push_back(currentFrameBlob.ptCurrentCenter);
    
    existingBlobs[intIndex].blnStillBeingTracked = true;
    existingBlobs[intIndex].blnCurrentMatchFoundOrNewBlob = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void addNewBlob(Blob &currentFrameBlob, std::vector<Blob> &existingBlobs) {
    //currentFrameBlob.vectorOfAllActualPoints.push_back(currentFrameBlob.ptCurrentCenter);
    currentFrameBlob.blnCurrentMatchFoundOrNewBlob = true;

    existingBlobs.push_back(currentFrameBlob);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
double distanceBetweenBlobs(Blob firstBlob, Blob secondBlob) {
    int intX = abs(firstBlob.ptCurrentCenter.x - secondBlob.ptCurrentCenter.x);
    int intY = abs(firstBlob.ptCurrentCenter.y - secondBlob.ptCurrentCenter.y);

    return(sqrt(pow(intX, 2) + pow(intY, 2)));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void drawBlobInfoOnImage(std::vector<Blob> &blobs, cv::Mat &imgFrame2Copy) {
    
    for (unsigned int i = 0; i < blobs.size(); i++) {

        if (blobs[i].blnStillBeingTracked == true) {
            cv::rectangle(imgFrame2Copy, blobs[i].boundingRect, SCALAR_RED, 2);

            int intFontFace = CV_FONT_HERSHEY_SIMPLEX;
            double dblFontScale = blobs[i].dblDiagonalSize / 60.0;
            int intFontThickness = (int)std::round(dblFontScale * 1.0);
            
            cv::putText(imgFrame2Copy, std::to_string(i), blobs[i].ptCurrentCenter, intFontFace, dblFontScale, SCALAR_GREEN, intFontThickness);
        }
    }
}










