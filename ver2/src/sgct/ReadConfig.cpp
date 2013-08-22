/*************************************************************************
Copyright (c) 2012-2013 Miroslav Andel
All rights reserved.

For conditions of distribution and use, see copyright notice in sgct.h 
*************************************************************************/

#define TIXML_USE_STL //needed for tinyXML lib to link properly in mingw
#define MAX_XML_DEPTH 16

#include "../include/sgct/ogl_headers.h"
#include "../include/sgct/ReadConfig.h"
#include "../include/sgct/MessageHandler.h"
#include "../include/sgct/ClusterManager.h"
#include "../include/external/tinyxml2.h"
#include "../include/sgct/SGCTSettings.h"

using namespace tinyxml2;

sgct_core::ReadConfig::ReadConfig( const std::string filename )
{
	valid = false;
	useExternalControlPort = false;

	//font stuff
	mFontSize = 10;
	#if __WIN32__
    mFontName = "verdanab.ttf";
    #elif __APPLE__
    mFontName = "Verdana Bold.ttf";
    #else
    mFontName = "FreeSansBold.ttf";
    #endif

	if( filename.empty() )
	{
		sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Error: No XML config file loaded.\n");
		return;
	}
	else
	{
	    sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Parsing XML config '%s'...\n", filename.c_str());
	}

    if( !replaceEnvVars(filename) )
        return;

	try
	{
		readAndParseXML();
	}
	catch(const char * err)
	{
		sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Error occured while reading config file '%s'\nError: %s\n", xmlFileName.c_str(), err);
		return;
	}
	valid = true;
	sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Config file '%s' read successfully!\n", xmlFileName.c_str());
	sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Number of nodes in cluster: %d\n",
		ClusterManager::Instance()->getNumberOfNodes());
	for(unsigned int i = 0; i<ClusterManager::Instance()->getNumberOfNodes(); i++)
		sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Node(%d) ip: %s [%s]\n", i,
		ClusterManager::Instance()->getNodePtr(i)->ip.c_str(),
		ClusterManager::Instance()->getNodePtr(i)->port.c_str());
}

bool sgct_core::ReadConfig::replaceEnvVars( const std::string &filename )
{
    size_t foundIndex = filename.find('%');
    if( foundIndex != std::string::npos )
    {
        sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Error: SGCT doesn't support the usage of '%%' characters in path or file name.\n");
		return false;
    }

    std::vector< size_t > beginEnvVar;
    std::vector< size_t > endEnvVar;

    foundIndex = 0;
    while( foundIndex != std::string::npos )
    {
        foundIndex = filename.find("$(", foundIndex);
        if(foundIndex != std::string::npos)
        {
            beginEnvVar.push_back(foundIndex);
            foundIndex = filename.find(')', foundIndex);
            if(foundIndex != std::string::npos)
                endEnvVar.push_back(foundIndex);
        }
    }

    if(beginEnvVar.size() != endEnvVar.size())
    {
        sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Error: Bad configuration path string!\n");
		return false;
    }
    else
    {
        size_t appendPos = 0;
        for(unsigned int i=0; i<beginEnvVar.size(); i++)
        {
            xmlFileName.append(filename.substr(appendPos, beginEnvVar[i] - appendPos));
            std::string envVar = filename.substr(beginEnvVar[i] + 2, endEnvVar[i] - (beginEnvVar[i] + 2) );
            char * fetchedEnvVar = NULL;

#if (_MSC_VER >= 1400) //visual studio 2005 or later
			size_t len;
			errno_t err = _dupenv_s( &fetchedEnvVar, &len, envVar.c_str() );
			if ( err )
			{
				sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Error: Cannot fetch environment variable '%s'.\n", envVar.c_str());
				return false;
			}
#else
			fetchedEnvVar = getenv(envVar.c_str());
			if( fetchedEnvVar == NULL )
			{
				sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Error: Cannot fetch environment variable '%s'.\n", envVar.c_str());
				return false;
			}
#endif

			xmlFileName.append( fetchedEnvVar );
            appendPos = endEnvVar[i]+1;
        }

        xmlFileName.append( filename.substr( appendPos ) );

        //replace all backslashes with slashes
        for(unsigned int i=0; i<xmlFileName.size(); i++)
            if(xmlFileName[i] == 92) //backslash
                xmlFileName[i] = '/';
    }

    return true;
}

void sgct_core::ReadConfig::readAndParseXML()
{
	if( xmlFileName.empty() )
        throw "Invalid XML file!";

	XMLDocument xmlDoc;
	if( xmlDoc.LoadFile(xmlFileName.c_str()) != XML_NO_ERROR )
	{
		throw "Invalid XML file!";
	}

	XMLElement* XMLroot = xmlDoc.FirstChildElement( "Cluster" );
	if( XMLroot == NULL )
	{
		throw "Cannot find XML root!";
	}

	const char * masterAddress = XMLroot->Attribute( "masterAddress" );
	if( masterAddress == NULL )
	{
		throw "Cannot find master address in XML!";
	}

	ClusterManager::Instance()->setMasterIp( masterAddress );

	if( XMLroot->Attribute( "externalControlPort" ) != NULL )
	{
		std::string tmpStr( XMLroot->Attribute( "externalControlPort" ) );
		ClusterManager::Instance()->setExternalControlPort(tmpStr);
		useExternalControlPort = true;
	}

	if( XMLroot->Attribute( "firmSync" ) != NULL )
	{
		ClusterManager::Instance()->setFirmFrameLockSyncStatus(
			strcmp( XMLroot->Attribute( "firmSync" ), "true" ) == 0 ? true : false );
	}

	XMLElement* element[MAX_XML_DEPTH];
	for(unsigned int i=0; i < MAX_XML_DEPTH; i++)
		element[i] = NULL;
	const char * val[MAX_XML_DEPTH];
	element[0] = XMLroot->FirstChildElement();
	while( element[0] != NULL )
	{
		val[0] = element[0]->Value();

        if( strcmp("Scene", val[0]) == 0 )
        {
            element[1] = element[0]->FirstChildElement();
			while( element[1] != NULL )
			{
				val[1] = element[1]->Value();

				if( strcmp("Offset", val[1]) == 0 )
				{
				    float tmpOffset[] = {0.0f, 0.0f, 0.0f};
					if( element[1]->QueryFloatAttribute("x", &tmpOffset[0] ) == XML_NO_ERROR &&
                        element[1]->QueryFloatAttribute("y", &tmpOffset[1] ) == XML_NO_ERROR &&
                        element[1]->QueryFloatAttribute("z", &tmpOffset[2] ) == XML_NO_ERROR)
                    {
                        glm::vec3 sceneOffset(1.0f);
						sceneOffset.x = tmpOffset[0];
                        sceneOffset.y = tmpOffset[1];
                        sceneOffset.z = tmpOffset[2];
                        sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Setting scene offset to (%f, %f, %f)\n",
                                                                sceneOffset.x,
                                                                sceneOffset.y,
                                                                sceneOffset.z);

						ClusterManager::Instance()->setSceneOffset( sceneOffset );
                    }
                    else
                        sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse scene offset from XML!\n");
				}
				else if( strcmp("Orientation", val[1]) == 0 )
				{
					float tmpOrientation[] = {0.0f, 0.0f, 0.0f};
					if( element[1]->QueryFloatAttribute("yaw", &tmpOrientation[0] ) == XML_NO_ERROR &&
                        element[1]->QueryFloatAttribute("pitch", &tmpOrientation[1] ) == XML_NO_ERROR &&
                        element[1]->QueryFloatAttribute("roll", &tmpOrientation[2] ) == XML_NO_ERROR)
                    {
                        float mYaw = glm::radians( tmpOrientation[0] );
                        float mPitch = glm::radians( tmpOrientation[1] );
                        float mRoll = glm::radians( tmpOrientation[2] );
                        sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Setting scene orientation to (%f, %f, %f) radians\n",
                                                                mYaw,
                                                                mPitch,
                                                                mRoll);

						ClusterManager::Instance()->setSceneRotation( mYaw, mPitch, mRoll );
                    }
                    else
                        sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse scene orientation from XML!\n");
				}
				else if( strcmp("Scale", val[1]) == 0 )
				{
					float tmpScale = 1.0f;
					if( element[1]->QueryFloatAttribute("value", &tmpScale ) == XML_NO_ERROR )
                    {
                        sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Setting scene scale to %f\n",
                                                                tmpScale );

						ClusterManager::Instance()->setSceneScale( tmpScale );
                    }
                    else
                        sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse scene orientation from XML!\n");
				}

				//iterate
				element[1] = element[1]->NextSiblingElement();
			}
        }
		else if( strcmp("Node", val[0]) == 0 )
		{
			SGCTNode tmpNode;
			tmpNode.ip.assign( element[0]->Attribute( "ip" ) );
			tmpNode.port.assign( element[0]->Attribute( "port" ) );

			element[1] = element[0]->FirstChildElement();
			while( element[1] != NULL )
			{
				val[1] = element[1]->Value();
				if( strcmp("Window", val[1]) == 0 )
				{
					SGCTWindow tmpWin;
					
					if( element[1]->Attribute("name") != NULL )
						tmpWin.setName( element[1]->Attribute("name") );

					if( element[1]->Attribute("fullscreen") != NULL )
						tmpWin.setWindowMode( strcmp( element[1]->Attribute("fullscreen"), "true" ) == 0 );

					int tmpSamples = 0;
					if( element[1]->QueryIntAttribute("numberOfSamples", &tmpSamples ) == XML_NO_ERROR && tmpSamples <= 128)
						tmpWin.setNumberOfAASamples(tmpSamples);
					else if( element[1]->QueryIntAttribute("msaa", &tmpSamples ) == XML_NO_ERROR && tmpSamples <= 128)
						tmpWin.setNumberOfAASamples(tmpSamples);

					if( element[1]->Attribute("fxaa") != NULL )
						tmpWin.setUseFXAA( strcmp( element[1]->Attribute("fxaa"), "true" ) == 0 ? true : false );

					if( element[1]->Attribute("swapLock") != NULL )
						tmpWin.setUseSwapGroups( strcmp( element[1]->Attribute("swapLock"), "true" ) == 0 ? true : false);

                    int tmpInterval = 0;
					if( element[1]->QueryIntAttribute("swapInterval", &tmpInterval) == XML_NO_ERROR )
						tmpWin.setSwapInterval( tmpInterval );

					if( element[1]->Attribute("decorated") != NULL )
						tmpWin.setWindowDecoration( strcmp( element[1]->Attribute("decorated"), "true" ) == 0 ? true : false);

					if( element[1]->Attribute("border") != NULL )
						tmpWin.setWindowDecoration( strcmp( element[1]->Attribute("border"), "true" ) == 0 ? true : false);

					int tmpMonitorIndex = 0;
					if( element[1]->QueryIntAttribute("monitor", &tmpMonitorIndex ) == XML_NO_ERROR)
						tmpWin.setFullScreenMonitorIndex( tmpMonitorIndex );

					element[2] = element[1]->FirstChildElement();
					while( element[2] != NULL )
					{
						val[2] = element[2]->Value();
						int tmpWinData[2];
						memset(tmpWinData,0,4);

						if( strcmp("Stereo", val[2]) == 0 )
						{
							tmpWin.setStereoMode( getStereoType( element[2]->Attribute("type") ) );
						}
						else if( strcmp("Pos", val[2]) == 0 )
						{
							if( element[2]->QueryIntAttribute("x", &tmpWinData[0] ) == XML_NO_ERROR &&
                                element[2]->QueryIntAttribute("y", &tmpWinData[1] ) == XML_NO_ERROR )
                                tmpWin.setWindowPosition(tmpWinData[0],tmpWinData[1]);
                            else
                                sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse window position from XML!\n");
						}
						else if( strcmp("Size", val[2]) == 0 )
						{
							if( element[2]->QueryIntAttribute("x", &tmpWinData[0] ) == XML_NO_ERROR &&
                                element[2]->QueryIntAttribute("y", &tmpWinData[1] ) == XML_NO_ERROR )
                                tmpWin.initWindowResolution(tmpWinData[0],tmpWinData[1]);
                            else
                                sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse window resolution from XML!\n");
						}
						else if( strcmp("Res", val[2]) == 0 )
						{
							if( element[2]->QueryIntAttribute("x", &tmpWinData[0] ) == XML_NO_ERROR &&
                                element[2]->QueryIntAttribute("y", &tmpWinData[1] ) == XML_NO_ERROR )
							{
                                tmpWin.setFramebufferResolution(tmpWinData[0],tmpWinData[1]);
								tmpWin.setFixResolution(true);
							}
                            else
                                sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse frame buffer resolution from XML!\n");
						}
						else if(strcmp("Viewport", val[2]) == 0)
						{
							Viewport tmpVp;

							if( element[2]->Attribute("name") != NULL )
								tmpVp.setName( element[2]->Attribute("name") );

							if( element[2]->Attribute("overlay") != NULL )
								tmpVp.setOverlayTexture( element[2]->Attribute("overlay") );

							if( element[2]->Attribute("mesh") != NULL )
								tmpVp.setCorrectionMesh( element[2]->Attribute("mesh") );

							if( element[2]->Attribute("tracked") != NULL )
								tmpVp.setTracked( strcmp( element[2]->Attribute("tracked"), "true" ) == 0 ? true : false );

							//get eye if set
							if( element[2]->Attribute("eye") != NULL )
							{
								if( strcmp("both", element[2]->Attribute("eye")) == 0 )
								{
									tmpVp.setEye(Frustum::Mono);
								}
								else if( strcmp("left", element[2]->Attribute("eye")) == 0 )
								{
									tmpVp.setEye(Frustum::StereoLeftEye);
								}
								else if( strcmp("right", element[2]->Attribute("eye")) == 0 )
								{
									tmpVp.setEye(Frustum::StereoRightEye);
								}
							}

							element[3] = element[2]->FirstChildElement();
							while( element[3] != NULL )
							{
								val[3] = element[3]->Value();
								double dTmp[2];
								dTmp[0] = 0.0;
								dTmp[1] = 0.0;

								if(strcmp("Pos", val[3]) == 0)
								{
									if( element[3]->QueryDoubleAttribute("x", &dTmp[0]) == XML_NO_ERROR &&
										element[3]->QueryDoubleAttribute("y", &dTmp[1]) == XML_NO_ERROR )
										tmpVp.setPos( dTmp[0], dTmp[1] );
									else
										sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse viewport position from XML!\n");
								}
								else if(strcmp("Size", val[3]) == 0)
								{
									if( element[3]->QueryDoubleAttribute("x", &dTmp[0]) == XML_NO_ERROR &&
										element[3]->QueryDoubleAttribute("y", &dTmp[1]) == XML_NO_ERROR )
										tmpVp.setSize( dTmp[0], dTmp[1] );
									else
										sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse viewport size from XML!\n");
								}
								else if(strcmp("Viewplane", val[3]) == 0)
								{
									element[4] = element[3]->FirstChildElement();
									while( element[4] != NULL )
									{
										val[4] = element[4]->Value();

										if( strcmp("Pos", val[4]) == 0 )
										{
											glm::vec3 tmpVec;
											float fTmp[3];
											static unsigned int i=0;
											if( element[4]->QueryFloatAttribute("x", &fTmp[0]) == XML_NO_ERROR &&
												element[4]->QueryFloatAttribute("y", &fTmp[1]) == XML_NO_ERROR &&
												element[4]->QueryFloatAttribute("z", &fTmp[2]) == XML_NO_ERROR )
											{
												tmpVec.x = fTmp[0];
												tmpVec.y = fTmp[1];
												tmpVec.z = fTmp[2];

												sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_DEBUG,
													"Adding view plane coordinates %f %f %f for plane %d\n",
													tmpVec.x, tmpVec.y, tmpVec.z, i%3);

												tmpVp.setViewPlaneCoords(i%3, tmpVec);
												i++;
											}
											else
												sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse view plane coordinates from XML!\n");
										}

										//iterate
										element[4] = element[4]->NextSiblingElement();
									}//end while level 3
								}//end if viewplane

								//iterate
								element[3] = element[3]->NextSiblingElement();
							}//end while level 2

							tmpWin.addViewport(tmpVp);
						}//end viewport
						else if(strcmp("Fisheye", val[2]) == 0)
						{
							float fov;
							if( element[2]->QueryFloatAttribute("fov", &fov) == XML_NO_ERROR )
								tmpWin.setFisheyeFOV( fov );

							if( element[2]->Attribute("quality") != NULL )
							{
								int resolution = getFisheyeCubemapRes( std::string(element[2]->Attribute("quality")) );
								if( resolution > 0 )
									tmpWin.setCubeMapResolution( resolution );
							}

							if( element[2]->Attribute("overlay") != NULL )
								tmpWin.setFisheyeOverlay( std::string(element[2]->Attribute("overlay")) );

							if( element[2]->Attribute("alpha") != NULL )
								tmpWin.setFisheyeAlpha( strcmp( element[2]->Attribute("alpha"), "true" ) == 0 ? true : false );

							float tilt;
							if( element[2]->QueryFloatAttribute("tilt", &tilt) == XML_NO_ERROR )
								tmpWin.setFisheyeTilt( tilt );

							float diameter;
							if( element[2]->QueryFloatAttribute("diameter", &diameter) == XML_NO_ERROR )
							{
								tmpWin.setDomeDiameter( diameter );
								sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Setting fisheye diameter to %f meters.\n", diameter);
							}

							element[3] = element[2]->FirstChildElement();
							while( element[3] != NULL )
							{
								val[3] = element[3]->Value();

								if( strcmp("Crop", val[3]) == 0 )
								{
									float tmpFArr[] = { 0.0f, 0.0f, 0.0f, 0.0f };
									float ftmp;

									if( element[3]->QueryFloatAttribute("left", &ftmp) == XML_NO_ERROR )
										tmpFArr[SGCTWindow::CropLeft] = ftmp;
									if( element[3]->QueryFloatAttribute("right", &ftmp) == XML_NO_ERROR )
										tmpFArr[SGCTWindow::CropRight] = ftmp;
									if( element[3]->QueryFloatAttribute("bottom", &ftmp) == XML_NO_ERROR )
										tmpFArr[SGCTWindow::CropBottom] = ftmp;
									if( element[3]->QueryFloatAttribute("top", &ftmp) == XML_NO_ERROR )
										tmpFArr[SGCTWindow::CropTop] = ftmp;

									tmpWin.setFisheyeCropValues(
										tmpFArr[SGCTWindow::CropLeft],
										tmpFArr[SGCTWindow::CropRight],
										tmpFArr[SGCTWindow::CropBottom],
										tmpFArr[SGCTWindow::CropTop]);
								}
								else if( strcmp("Offset", val[3]) == 0 )
								{
									float tmpFArr[] = { 0.0f, 0.0f, 0.0f };
									float ftmp;

									if( element[3]->QueryFloatAttribute("x", &ftmp) == XML_NO_ERROR )
										tmpFArr[0] = ftmp;
									if( element[3]->QueryFloatAttribute("y", &ftmp) == XML_NO_ERROR )
										tmpFArr[1] = ftmp;
									if( element[3]->QueryFloatAttribute("z", &ftmp) == XML_NO_ERROR )
										tmpFArr[2] = ftmp;

									tmpWin.setFisheyeBaseOffset(tmpFArr[0], tmpFArr[1], tmpFArr[2]);
								}

								//iterate
								element[3] = element[3]->NextSiblingElement();
							}

							tmpWin.setFisheyeRendering(true);

						}//end fisheye

						//iterate
						element[2] = element[2]->NextSiblingElement();
					}

					tmpNode.addWindow( tmpWin );
				}//end window

				//iterate
				element[1] = element[1]->NextSiblingElement();
			
			}//end while

			ClusterManager::Instance()->addNode(tmpNode);
		}//end if node
		else if( strcmp("User", val[0]) == 0 )
		{
			float fTmp;
			if( element[0]->QueryFloatAttribute("eyeSeparation", &fTmp) == XML_NO_ERROR )
                ClusterManager::Instance()->getUserPtr()->setEyeSeparation( fTmp );
            else
                sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse user eye separation from XML!\n");

			element[1] = element[0]->FirstChildElement();
			while( element[1] != NULL )
			{
				val[1] = element[1]->Value();

				if( strcmp("Pos", val[1]) == 0 )
				{
					double dTmp[3];
					if( element[1]->QueryDoubleAttribute("x", &dTmp[0]) == XML_NO_ERROR &&
                        element[1]->QueryDoubleAttribute("y", &dTmp[1]) == XML_NO_ERROR &&
                        element[1]->QueryDoubleAttribute("z", &dTmp[2]) == XML_NO_ERROR )
                        ClusterManager::Instance()->getUserPtr()->setPos(dTmp);
                    else
                        sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse user position from XML!\n");
				}
				else if( strcmp("Orientation", val[1]) == 0 )
				{
					float fTmp[3];
					if( element[1]->QueryFloatAttribute("x", &fTmp[0]) == XML_NO_ERROR &&
                        element[1]->QueryFloatAttribute("y", &fTmp[1]) == XML_NO_ERROR &&
                        element[1]->QueryFloatAttribute("z", &fTmp[2]) == XML_NO_ERROR )
                        ClusterManager::Instance()->getUserPtr()->setOrientation(fTmp[0], fTmp[1], fTmp[2]);
                    else
                        sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse user Orientation from XML!\n");
				}
				else if( strcmp("Tracking", val[1]) == 0 )
				{
					if(	element[1]->Attribute("tracker") != NULL &&
						element[1]->Attribute("device") != NULL )
					{
						ClusterManager::Instance()->getUserPtr()->setHeadTracker(
							element[1]->Attribute("tracker"), element[1]->Attribute("device") );
					}
					else
						sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse user tracking data from XML!\n");
				}

				//iterate
				element[1] = element[1]->NextSiblingElement();
			}
		}//end user
		else if( strcmp("Settings", val[0]) == 0 )
		{
			element[1] = element[0]->FirstChildElement();
			while( element[1] != NULL )
			{
				val[1] = element[1]->Value();

				if( strcmp("DepthBufferTexture", val[1]) == 0 )
				{
					if( element[1]->Attribute("value") != NULL )
						sgct::SGCTSettings::Instance()->setUseDepthTexture( strcmp( element[1]->Attribute("value"), "true" ) == 0 ? true : false );
				}
				else if( strcmp("FXAA", val[1]) == 0 )
				{
					float offset = 0.0f;
					if( element[1]->QueryFloatAttribute("offset", &offset) == XML_NO_ERROR)
					{
						sgct::SGCTSettings::Instance()->setFXAASubPixOffset( offset );
						sgct::MessageHandler::Instance()->print( sgct::MessageHandler::NOTIFY_INFO, 
							"ReadConfig: Setting FXAA sub-pixel offset to %f\n", offset );
					}

					float trim = 0.0f;
					if( element[1]->QueryFloatAttribute("trim", &trim) == XML_NO_ERROR )
					{
						if(trim > 0.0f)
						{
							sgct::SGCTSettings::Instance()->setFXAASubPixTrim( 1.0f/trim );
							sgct::MessageHandler::Instance()->print( sgct::MessageHandler::NOTIFY_INFO, 
								"ReadConfig: Setting FXAA sub-pixel trim to %f\n", 1.0f/trim );
						}
						else
						{
							sgct::SGCTSettings::Instance()->setFXAASubPixTrim( 0.0f );
							sgct::MessageHandler::Instance()->print( sgct::MessageHandler::NOTIFY_INFO, 
								"ReadConfig: Setting FXAA sub-pixel trim to %f\n", 0.0f );
						}
					}
				}

				//iterate
				element[1] = element[1]->NextSiblingElement();
			}
		}//end settings
		else if( strcmp("Font", val[0]) == 0 )
		{
			if( element[0]->Attribute("name") != NULL )
			{
			    mFontName.assign( element[0]->Attribute("name") );
            }

            if( element[0]->Attribute("path") != NULL )
			{
			    mFontPath.assign( element[0]->Attribute("path") );
            }

            if( element[0]->Attribute("size") != NULL )
			{
                int tmpi = -1;
				if( element[0]->QueryIntAttribute("size", &tmpi) == XML_NO_ERROR && tmpi > 0)
				{
					mFontSize = tmpi;
				}
				else
					sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Info: Font size not specified. Setting to default size=10!\n");
            }
        }
		else if( strcmp("Capture", val[0]) == 0 )
		{
			if( element[0]->Attribute("path") != NULL )
			{
			    sgct::SGCTSettings::Instance()->setCapturePath( element[0]->Attribute("path"), sgct::SGCTSettings::Mono );
				sgct::SGCTSettings::Instance()->setCapturePath( element[0]->Attribute("path"), sgct::SGCTSettings::LeftStereo );
				sgct::SGCTSettings::Instance()->setCapturePath( element[0]->Attribute("path"), sgct::SGCTSettings::RightStereo );
            }
			if( element[0]->Attribute("monoPath") != NULL )
			{
			    sgct::SGCTSettings::Instance()->setCapturePath( element[0]->Attribute("monoPath"), sgct::SGCTSettings::Mono );
            }
			if( element[0]->Attribute("leftPath") != NULL )
			{
			    sgct::SGCTSettings::Instance()->setCapturePath( element[0]->Attribute("leftPath"), sgct::SGCTSettings::LeftStereo );
            }
			if( element[0]->Attribute("rightPath") != NULL )
			{
			    sgct::SGCTSettings::Instance()->setCapturePath( element[0]->Attribute("rightPath"), sgct::SGCTSettings::RightStereo );
            }

            if( element[0]->Attribute("format") != NULL )
			{
			    sgct::SGCTSettings::Instance()->setCaptureFormat( element[0]->Attribute("format") );
            }
        }
		else if( strcmp("Tracker", val[0]) == 0 && element[0]->Attribute("name") != NULL )
		{
			ClusterManager::Instance()->getTrackingManagerPtr()->addTracker( std::string(element[0]->Attribute("name")) );

			element[1] = element[0]->FirstChildElement();
			while( element[1] != NULL )
			{
				val[1] = element[1]->Value();

				if( strcmp("Device", val[1]) == 0 && element[1]->Attribute("name") != NULL)
				{
					ClusterManager::Instance()->getTrackingManagerPtr()->addDeviceToCurrentTracker( std::string(element[1]->Attribute("name")) );

					element[2] = element[1]->FirstChildElement();

					while( element[2] != NULL )
					{
						val[2] = element[2]->Value();
						unsigned int tmpUI = 0;
						int tmpi = -1;

						if( strcmp("Sensor", val[2]) == 0 )
						{
							if( element[2]->Attribute("vrpnAddress") != NULL &&
                                element[2]->QueryIntAttribute("id", &tmpi) == XML_NO_ERROR )
							{
                                ClusterManager::Instance()->getTrackingManagerPtr()->addSensorToCurrentDevice(
                                    element[2]->Attribute("vrpnAddress"), tmpi);
							}
						}
						else if( strcmp("Buttons", val[2]) == 0 )
						{
							if(element[2]->Attribute("vrpnAddress") != NULL &&
                                element[2]->QueryUnsignedAttribute("count", &tmpUI) == XML_NO_ERROR )
                            {
                                ClusterManager::Instance()->getTrackingManagerPtr()->addButtonsToCurrentDevice(
                                    element[2]->Attribute("vrpnAddress"), tmpUI);
                            }

						}
						else if( strcmp("Axes", val[2]) == 0 )
						{
							if(element[2]->Attribute("vrpnAddress") != NULL &&
                                element[2]->QueryUnsignedAttribute("count", &tmpUI) == XML_NO_ERROR )
                            {
                                ClusterManager::Instance()->getTrackingManagerPtr()->addAnalogsToCurrentDevice(
                                    element[2]->Attribute("vrpnAddress"), tmpUI);
                            }
						}
						else if( strcmp("Offset", val[2]) == 0 )
						{
							double tmpd[3];
							if( element[2]->QueryDoubleAttribute("x", &tmpd[0]) == XML_NO_ERROR &&
								element[2]->QueryDoubleAttribute("y", &tmpd[1]) == XML_NO_ERROR &&
								element[2]->QueryDoubleAttribute("z", &tmpd[2]) == XML_NO_ERROR )
								ClusterManager::Instance()->getTrackingManagerPtr()->getLastTrackerPtr()->getLastDevicePtr()->
									setOffset( tmpd[0], tmpd[1], tmpd[2] );
							else
								sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse device offset in XML!\n");
						}
						else if( strcmp("Orientation", val[2]) == 0 )
						{
							double tmpd[3];
							if( element[2]->QueryDoubleAttribute("x", &tmpd[0]) == XML_NO_ERROR &&
								element[2]->QueryDoubleAttribute("y", &tmpd[1]) == XML_NO_ERROR &&
								element[2]->QueryDoubleAttribute("z", &tmpd[2]) == XML_NO_ERROR )
								ClusterManager::Instance()->getTrackingManagerPtr()->getLastTrackerPtr()->getLastDevicePtr()->
									setOrientation( tmpd[0], tmpd[1], tmpd[2] );
							else
								sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse device orientation in XML!\n");
						}

						//iterate
						element[2] = element[2]->NextSiblingElement();
					}

				}
				else if( strcmp("Offset", val[1]) == 0 )
				{
					double tmpd[3];
					if( element[1]->QueryDoubleAttribute("x", &tmpd[0]) == XML_NO_ERROR &&
                        element[1]->QueryDoubleAttribute("y", &tmpd[1]) == XML_NO_ERROR &&
                        element[1]->QueryDoubleAttribute("z", &tmpd[2]) == XML_NO_ERROR )
						ClusterManager::Instance()->getTrackingManagerPtr()->getLastTrackerPtr()->setOffset( tmpd[0], tmpd[1], tmpd[2] );
                    else
                        sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse tracker offset in XML!\n");
				}
				else if( strcmp("Orientation", val[1]) == 0 )
				{
					double tmpd[3];
					if( element[1]->QueryDoubleAttribute("x", &tmpd[0]) == XML_NO_ERROR &&
                        element[1]->QueryDoubleAttribute("y", &tmpd[1]) == XML_NO_ERROR &&
                        element[1]->QueryDoubleAttribute("z", &tmpd[2]) == XML_NO_ERROR )
						ClusterManager::Instance()->getTrackingManagerPtr()->getLastTrackerPtr()->setOrientation( tmpd[0], tmpd[1], tmpd[2] );
                    else
                        sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse tracker orientation in XML!\n");
				}
				else if( strcmp("Scale", val[1]) == 0 )
				{
					double scaleVal;
					if( element[1]->QueryDoubleAttribute("value", &scaleVal) == XML_NO_ERROR )
						ClusterManager::Instance()->getTrackingManagerPtr()->getLastTrackerPtr()->setScale( scaleVal );
                    else
                        sgct::MessageHandler::Instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to parse tracker scale in XML!\n");
				}

				//iterate
				element[1] = element[1]->NextSiblingElement();
			}
		}// end tracking part

		//iterate
		element[0] = element[0]->NextSiblingElement();
	}
}

sgct_core::SGCTWindow::StereoMode sgct_core::ReadConfig::getStereoType( const std::string type )
{
	if( strcmp( type.c_str(), "none" ) == 0 )
		return SGCTWindow::NoStereo;
	else if( strcmp( type.c_str(), "active" ) == 0 )
		return SGCTWindow::Active;
	else if( strcmp( type.c_str(), "checkerboard" ) == 0 )
		return SGCTWindow::Checkerboard;
	else if( strcmp( type.c_str(), "checkerboard_inverted" ) == 0 )
		return SGCTWindow::Checkerboard_Inverted;
	else if( strcmp( type.c_str(), "anaglyph_red_cyan" ) == 0 )
		return SGCTWindow::Anaglyph_Red_Cyan;
	else if( strcmp( type.c_str(), "anaglyph_amber_blue" ) == 0 )
		return SGCTWindow::Anaglyph_Amber_Blue;
	else if( strcmp( type.c_str(), "anaglyph_wimmer" ) == 0 )
		return SGCTWindow::Anaglyph_Red_Cyan_Wimmer;
	else if( strcmp( type.c_str(), "vertical_interlaced" ) == 0 )
		return SGCTWindow::Vertical_Interlaced;
	else if( strcmp( type.c_str(), "vertical_interlaced_inverted" ) == 0 )
		return SGCTWindow::Vertical_Interlaced_Inverted;
	else if( strcmp( type.c_str(), "test" ) == 0 || strcmp( type.c_str(), "dummy" ) == 0 )
		return SGCTWindow::DummyStereo;

	//if match not found
	return SGCTWindow::NoStereo;
}

int sgct_core::ReadConfig::getFisheyeCubemapRes( const std::string quality )
{
	if( strcmp( quality.c_str(), "low" ) == 0 )
		return 256;
	else if( strcmp( quality.c_str(), "medium" ) == 0 )
		return 512;
	else if( strcmp( quality.c_str(), "high" ) == 0 || strcmp( quality.c_str(), "1k" ) == 0 )
		return 1024;
	else if( strcmp( quality.c_str(), "2k" ) == 0 )
		return 2048;
	else if( strcmp( quality.c_str(), "4k" ) == 0 )
		return 4096;
	else if( strcmp( quality.c_str(), "8k" ) == 0 )
		return 8192;

	//if match not found
	return -1;
}