/*
 * Copyright (C) 2020 Global Graphics Software Ltd. All rights reserved
 *
 *  Simple sample application to report on job parameters in PJL-wrapped PCL, PCL/XL or Postscript, using Mako APIs.
 */

#include <algorithm>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <jawsmako/jawsmako.h>
#include <jawsmako/psinput.h>
#include <jawsmako/pcl5input.h>
#include <jawsmako/pclxlinput.h>
#include <jawsMako/pdfoutput.h>
#include <filesystem>

using namespace JawsMako;
using namespace EDL;

void GetAttributes(const IPJLParserPtr &pjlParser)
{
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
	        const PValue pVal = jobTkValue->getValue();
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
	            if (childNode->getJobTkNodeType() == IDOMJobTkNode::eDOMJobTkPTNodeOption)
	            {
	                qName = childNode->getQName();
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

    sTestFile(String f, eFileFormat e);
};

sTestFile::sTestFile(String f, eFileFormat e): filePath(f), type(e)
{
}

int main(int argc, char *argv[])
{
    if (argc < 1)
    {
        std::wcout << L"Usage: makopjltest <path\\to\\folder\\of\\testfiles> [-c]" << std::endl;
        std::wcout << L"  Specify -c to also convert to PDF (in a folder named PDF)" << std::endl;
        return 1;
    }

    // Was a second parameter specified?
    bool convertToPDF = false;
	if (argc > 2)
    {
        U8String param = argv[2];
        std::transform(param.begin(), param.end(), param.begin(), towlower);
        convertToPDF = param == "-c";
    }
	
	// Find all files in the folder specified as the first argument. Does not recurse into folders.
	std::vector <sTestFile> testFiles;
	for (const auto& entry : std::filesystem::directory_iterator(argv[1]))
    {
		if(entry.is_regular_file())
			testFiles.emplace_back(sTestFile(entry.path().c_str(), eFFUnknown));
    }

	try
    {
        // Create JawsMako instance
        const IJawsMakoPtr jawsMako = IJawsMako::create(".");
        IJawsMako::enableAllFeatures(jawsMako);

#if (-1)
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
                    switch (pjlResult)
                    {
	                    case IPJLParser::ePREnterPostScript:
	                    {
	                        testFiles[index].type = eFFPS;
	                        GetAttributes(pjlParser);
	                        break;
	                    }

	                    case IPJLParser::ePREnterPclXl:
	                    {
	                        testFiles[index].type = eFFPCLXL;
	                        GetAttributes(pjlParser);
	                        break;
	                    }

	                    case IPJLParser::ePREnterPcl:
	                    {
	                        testFiles[index].type = eFFPCL5;
	                        GetAttributes(pjlParser);
	                        break;
	                    }

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
                if (testFiles[index].type == eFFPCL5)
                    std::cout << "PCL5 " ;
                if (testFiles[index].type == eFFPCLXL)
                    std::cout << "PCL/XL " ;
                if (testFiles[index].type == eFFPS)
                    std::cout << "PostScript " ;
                std::wcout << "End of PJL" << std::endl;

            }
            catch (IError & e)
            {
                uint32 x = e.getErrorCode();
                if (x == 124)
                    continue;
                
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
        }
#endif

        // *** Example 2: Process the print tickets ***

        // Look at each test file
		for (sTestFile testFile : testFiles)
        {
            // Open the stream.
	        // The PJL parser requires a stream that implements the IInputPushbackStream
	        // interface, as it needs to sniff content to do its job. We can overlay
	        // this on a standard file stream.
            IInputPushbackStreamPtr prnStream = IInputStream::createPushbackStream(jawsMako, IInputStream::createFromFile(jawsMako, testFile.filePath));

            // Create our PJL Parser, PCL/5, PCL/XL and PS inputs
            IPJLParserPtr  pjlParser = IPJLParser::create(jawsMako);
            IPCLXLInputPtr xlInput = IPCLXLInput::create(jawsMako);
            IPCL5InputPtr  pcl5Input = IPCL5Input::create(jawsMako);
            IPSInputPtr    psInput = IPSInput::create(jawsMako);

            // Normally the PCLXL and PCL5 inputs will process PJL themselves. We want
            // to take control, so we use them unencapsulated.
            xlInput->enableUnencapsulatedMode(true);
            pcl5Input->enableUnencapsulatedMode(true);
            // The PostScript input does not handle PJL itself, but we still
            // do not want it to open the input stream. The API however is the
            // same.
            psInput->enableUnencapsulatedMode(true);

            // Open the stream
            if (!prnStream->open())
            {
                throw std::runtime_error("Could not open input stream");
            }

            // Create an output for saving the file as a PDF
            IPDFOutputPtr output = IPDFOutput::create(jawsMako);

            IInputPtr input;
            
			// Now start parsing until we run out of input, beginning in PJL mode
            IPJLParser::ePjlResult pjlResult;

            try {
                // Repeatedly parse PJL until we get to the end of the stream
                while ((pjlResult = pjlParser->parse(prnStream)) != IPJLParser::ePREndOfFile)
                {
                    // We have a PCL/XL, PCL5e or PostScript stream. Open.
                    switch (pjlResult)
                    {
                    case IPJLParser::ePREnterPclXl:
                        input = xlInput;
                        break;

                    case IPJLParser::ePREnterPcl:
                        input = pcl5Input;
                        break;

                    case IPJLParser::ePREnterPostScript:
                        input = psInput;
                        break;

                    default:
                        // Should not occur
                        throw std::runtime_error("Unexpected PJL Result");
                    }

                    // Open this portion of the stream.
                    IDocumentAssemblyPtr assembly = input->open(prnStream);
                    {
                        // Get assembly-level print ticket
                        std::wcout << L"\nFile " << testFile.filePath << L":" << std::endl;
                        std::wcout << L"  Assembly-level print ticket:" << std::endl;
                        IDOMJobTkPtr jobTicket = assembly->getJobTicket();
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
                    }

                    // Look at each document
                    for (int documentIndex = 0; documentIndex < assembly->getNumDocuments(); documentIndex++)
                    {
                        IDocumentPtr document = assembly->getDocument(documentIndex);
                        uint32 numPages = document->getNumPages();
                        if (numPages > 0)
                        {
                            std::wcout << L"  Document " << documentIndex + 1 << L" of " << assembly->getNumDocuments() << L":" << std::endl;

                            // Get document-level print ticket
                            std::wcout << L"    Document-level print ticket:" << std::endl;
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
                                std::wcout << L"      ** None found **" << std::endl;

                            // Look at each page
                            for (uint32 pageIndex = 0; pageIndex < numPages; pageIndex++)
                            {
                                std::wcout << L"    Page-level print ticket for page " << pageIndex + 1 << " of " << numPages << L":" << std::endl;
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
                                    std::wcout << L"      ** None found **" << std::endl;
                            }
                        }

                        // Write the document out to a 'PDF' folder
                        if (convertToPDF) {
                            std::filesystem::path path = testFile.filePath;
                            auto outFile = path.filename().replace_extension(".pdf");
                            String outFolder = path.remove_filename().append("PDF\\").c_str();
                            std::filesystem::create_directory(outFolder);
                            output->writeAssembly(assembly, outFolder + outFile.c_str());
                        }
                    }
                }
            }
            catch(IError &e)
            {
                // PJL exhausted?
                if (e.getErrorCode() == 124)
                    continue;

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
