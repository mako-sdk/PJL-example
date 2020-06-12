/*
 * Copyright (C) 2020 Global Graphics Software Ltd. All rights reserved
 *
 *  Simple sample application to report on job parameters in PCL, using Mako APIs.
 */

#include <algorithm>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <jawsmako/jawsmako.h>
#include <jawsmako/xpsoutput.h>
#include <jawsmako/pcl5input.h>

using namespace JawsMako;
using namespace EDL;

static bool ReportJobTicketItem(const IDOMJobTkNodePtr& node)
{
    // Get name (without namespace prefix)
    EDLQName qName = node->getQName();
    std::wcout << L"    Parameter " << qName.getName() << L" \tValue ";

    const IDOMJobTkNode::eDOMJobTkNodeType nodeType = node->getJobTkNodeType();
    
	switch(nodeType)
	{
	    case IDOMJobTkNode::eDOMJobTkPTNodeParameterInit:
	    {
	        // Report value
	        const IDOMJobTkValuePtr jobTkValue = node->getChildValue();
            PValue pVal = jobTkValue->getValue();
            switch (pVal.getType())
            {
	            case PValue::T_UNASSIGNED:
	                std::wcout << L"** not available **" << std::endl;
	            break;

	            case PValue::T_INT:
	                std::wcout << pVal.getInt32() << std::endl;
	            break;

	            case PValue::T_STRING:
	                std::wcout << pVal.getString() << std::endl;
	            break;

	            case PValue::T_QNAME:
	                std::wcout << pVal.getQName().getName() << std::endl;
	            break;

                default:
                    std::wcout << L"** value not available **" << std::endl;
            }
	    }
        break;

	    case IDOMJobTkNode::eDOMJobTkPTNodeFeature:
	    {
	        // Report option
	        IDOMJobTkNodePtr childNode = edlobj2IDOMJobTkNode(node->getFirstChild());
	        if (childNode)
            {
	            auto nodeType = childNode->getJobTkNodeType();
	            if (childNode->getJobTkNodeType() == IDOMJobTkNode::eDOMJobTkPTNodeOption)
	            {
	                EDLQName qName = childNode->getQName();
	                std::wcout << qName.getName() << std::endl;
	            }
	            return true;
			}
	        std::wcout << L"** not available **" << std::endl;
	        return false;
	    }

	    default:
	        std::wcout << L"Node type " << nodeType << std::endl;
	}
	
    return true;
}

struct sTestFile
{
    String filePath;
    eFileFormat type;

    sTestFile(String f, eFileFormat e) : filePath(f), type(e) 
	{ }
};

#ifdef _WIN32
int wmain(int argc, wchar_t *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    std::vector <sTestFile> testFiles;
    testFiles.emplace_back(sTestFile(L"printtickettestspjl.pcl", eFFUnknown));

	try
    {
        // Create JawsMako instance
        const IJawsMakoPtr jawsMako = IJawsMako::create(".");
        IJawsMako::enableAllFeatures(jawsMako);

		// *** Example 1: Process the PJL header only ***
        for (uint32 index = 0; index < testFiles.size(); index++)
        {
        	// Get the stream ready
            IInputPushbackStreamPtr prnStream = IInputStream::createPushbackStream(jawsMako, IInputStream::createFromFile(jawsMako, testFiles[index].filePath));
            IPJLParserPtr pjlParser = IPJLParser::create(jawsMako);

            // Open the stream
            if (!prnStream->open())
            {
                throw std::runtime_error("Could not open input stream");
            }

            std::wcout << testFiles[index].filePath << L": ";

            IPJLParser::ePjlResult pjlResult;
            // Repeatedly parse PJL until we get to the end of the stream
            try {
                while ((pjlResult = pjlParser->parse(prnStream)) != IPJLParser::ePREndOfFile)
                {
                    testFiles[index].type = eFFPCL5;
                    switch (pjlResult)
                    {
                    case IPJLParser::ePREnterPclXl:
                        testFiles[index].type = eFFPCLXL;
                    case IPJLParser::ePREnterPcl:
                    {
                   		std::wcout << (testFiles[index].type == eFFPCL5 ? "PCL5" : "PCL/XL ") ;
                        IPJLParser::CPjlAttributeVect attributes = pjlParser->getAttributes("SET", "DUPLEX");
                        if (attributes.size() != 0)
                        {
                            // Take the last-seen one if there are multiples
                            RawString value = attributes[attributes.size() - 1].value;

                            // Case insensitive
                            std::transform(value.begin(), value.end(), value.begin(), ::tolower);

                            if (value == "on")
                            {
                                // What is the direction?
                                String duplexMode = L"TwoSidedLongEdge";

                                // Look for Binding direction in the PJL.
                                attributes = pjlParser->getAttributes("SET", "BINDING");
                                if (attributes.size() != 0)
                                {
                                    // Take the last-seen one if there are multiples
                                    value = attributes[attributes.size() - 1].value;

                                    // Case insensitive
                                    std::transform(value.begin(), value.end(), value.begin(), ::tolower);

                                    if (value == "longedge")
                                    {
                                        duplexMode = L"TwoSidedLongEdge";
                                    }
                                    else if (value == "shortedge")
                                    {
                                        duplexMode = L"TwoSidedShortEdge";
                                    }
                                }

                                std::wcout << "Duplex mode: " << duplexMode << std::endl;
                            }
                            else
                                std::wcout << "Duplex mode: OFF" << std::endl;
                        }
                    }
                    break;

                    case IPJLParser::ePREndOfFile:
                        break;

                    default:
                        // Should not occur
                        throw std::runtime_error("Unexpected PJL Result");
                    }

                    // If we have hit PCL, then break out
                    if (pjlResult != IPJLParser::ePREndOfFile)
                        break;
                }

                // Reached end of PJL
                std::wcout << "End of PJL" << std::endl;
            }
            catch (IError & e)
            {
                uint32 x = e.getErrorCode();
                if (x == 124)
                    continue;
            	throwEDLError(e.getErrorCode());
            }
        }

        // *** Example 2: Process the print tickets ***

        // Look at each test file
		for (sTestFile testFile : testFiles)
        {
            IInputPtr input = IInput::create(jawsMako, testFile.type);
			IDocumentAssemblyPtr assembly = input->open(testFile.filePath);
            
			// Look at each document
			for (int documentIndex = 0; documentIndex < assembly->getNumDocuments(); documentIndex++)
            {
                IDocumentPtr document = assembly->getDocument(documentIndex);
                uint32 numPages = document->getNumPages();
                if (numPages > 0)
                {
                    std::wcout << L"Document " << documentIndex + 1 << L" in " << testFile.filePath << L":" << std::endl;

                    // Get document-level print ticket
                    std::wcout << L"  Document-level print ticket:" << std::endl;
                    IDOMJobTkPtr jobTicket = document->getJobTicket();
                    if (jobTicket)
                    {
                        IDOMJobTkContentPtr jobTicketContent = jobTicket->getContent();
                        IDOMJobTkNodePtr node = jobTicketContent->getRootNode();
                        while (node)
                        {
                            ReportJobTicketItem(node);
                            node = edlobj2IDOMJobTkNode(node->getNextSibling());
                        }
                    }
                    else
                        std::wcout << L"    ** None found **" << std::endl;

                    // Look at each page
                    for (uint32 pageIndex = 0; pageIndex < numPages; pageIndex++)
                    {
                        std::wcout << L"  Page-level print ticket for page " << pageIndex + 1 << L":" << std::endl;
                        IPagePtr page = document->getPage(pageIndex);
                        IDOMJobTkPtr pageJobTicket = page->getJobTicket();
                        if (pageJobTicket)
                        {
                            IDOMJobTkContentPtr pageJobTicketContent = pageJobTicket->getContent();
                            IDOMJobTkNodePtr node = pageJobTicketContent->getRootNode();

                            while (node)
                            {
                                ReportJobTicketItem(node);
                                node = edlobj2IDOMJobTkNode(node->getNextSibling());
                            }
                        }
                        else
                            std::wcout << L"  ** None found **" << std::endl;
                    }
                }
            }
        }
    }
    catch (IError &e)
    {
        String errorFormatString = getEDLErrorString(e.getErrorCode());
        std::wcerr << L"Exception thrown: " << e.getErrorDescription(errorFormatString) << std::endl;
#ifdef _WIN32
        // On windows, the return code allows larger numbers, and we can return the error code
        return e.getErrorCode();
#else
        // On other platforms, the exit code is masked to the low 8 bits. So here we just return
        // a fixed value.
        return 1;
#endif
    }
    catch (std::exception &e)
    {
        std::wcerr << L"std::exception thrown: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
