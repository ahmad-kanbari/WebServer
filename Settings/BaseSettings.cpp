
#include "BaseSettings.hpp"
#include <algorithm>

BaseSettings::BaseSettings(std::string& HttpRoot, 
							std::string& HttpAutoIndex, 
							std::string& HttpClientMaxBodySize,
							std::string& HttpKeepaliveTimeout,
							std::string& HttpErrorPagesContext,
							std::vector<DirectiveNode* >& HttpErrorArgs, 
							std::vector<DirectiveNode* >& HttpIndexArgs)
{
	if (!HttpRoot.empty())
		setRoot(HttpRoot);

	if (!HttpAutoIndex.empty())
		setAutoIndex(HttpAutoIndex);

	if (!HttpClientMaxBodySize.empty())
		setClientMaxBodySize(HttpClientMaxBodySize);
	else
		clientMaxBodySize = std::numeric_limits<size_t>::max();

	for (size_t i = 0; i < HttpErrorArgs.size(); i++)
		setErrorPages(HttpErrorArgs[i]->getArguments(), HttpErrorPagesContext);
	
	if (!HttpKeepaliveTimeout.empty())
		setKeepAliveTimeout(HttpKeepaliveTimeout);
	else
		keepaliveTimeout = std::numeric_limits<size_t>::max();

	for (size_t i = 0; i < HttpIndexArgs.size(); i++)
		setIndex(HttpIndexArgs[i]->getArguments());
}

BaseSettings::BaseSettings(std::string serverRoot, 
							std::string serverAutoIndex, 
							size_t serverClientMaxBodySize,
							size_t serverKeepaliveTimeout,
							std::map<int, std::string> serverErrorPages, 
							std::map<int, std::string> serverErrorPagesLevel, 
							std::vector<std::string> serverIndex) :
								root(serverRoot), 
								autoIndex(serverAutoIndex), 
								clientMaxBodySize(serverClientMaxBodySize),
								keepaliveTimeout(serverKeepaliveTimeout),
								errorPages(serverErrorPages), 
								errorPagesLevel(serverErrorPagesLevel), 
								index(serverIndex) 
{
}

BaseSettings::BaseSettings(const BaseSettings& other)
{
	*this = other;
}

BaseSettings::~BaseSettings()
{
	;
}

BaseSettings& BaseSettings::operator=(const BaseSettings& other)
{
	if (this != &other)
	{
		this->root = other.root;
		this->autoIndex = other.autoIndex;
		this->clientMaxBodySize = other.clientMaxBodySize;
		this->keepaliveTimeout = other.keepaliveTimeout;
		this->errorPages = other.errorPages;
		this->errorPagesLevel = other.errorPagesLevel;
		this->index = other.index;
		this->returnDirective = other.returnDirective;
	}
	return (*this);
}

/*
	Notes:
		Note 1: The idea is that we will later append the URI from
				the request to the root from the config file. So 
				since the request always contains a slash for the 
				URI in a request line, like "GET /"" or "GET /content", 
				then we do not want to have a double slash in the full 
				path. Thus, we just detach that slash from the root if 
				the user inputted a slash at the end.
*/
void BaseSettings::setRoot(const std::string& root)
{
	if (!root.empty()) {
        std::string::const_reverse_iterator rit = root.rbegin();
        std::string::const_reverse_iterator rend = root.rend();

        // Find the position of the last non-slash character
        while (rit != rend && *rit == '/')
            ++rit; // Move to the next character if it's a slash

        // Create the new root string without trailing slashes
        this->root = std::string(root.begin(), rit.base()); // Use base() to convert reverse iterator to normal iterator
    }
	else 
    	this->root = root; // Assign empty root if the input is empty
}

void	BaseSettings::setAutoIndex(const std::string& autoIndex)
{
	if (autoIndex.empty() || (autoIndex != "on" && autoIndex != "off"))
		throw (std::runtime_error("invalid value \"" + autoIndex + "\" in \"autoindex\" directive"));
	this->autoIndex = autoIndex;
}

/*
	RULES:
		Accepted chars: only is_alphanum and for alpha, k (K), m (M), or g (G) and no signs '-' or '+'
		Units: none (bytes), k or K (kilobytes), m or M (megabytes), g or G (gigabytes)

		Whitespaces: not allowed

		Default value: 1M

		Max value: size_t max

	NOTES:
		Note 1 : If there is a non-digit value, it can only be accepted if it is at the end of the string

		Note 2 : That letter also has to be either m (M), k (K), or g (G)

		Note 3 : Check that the size does not exceed the maximum size of size_t
*/
void	BaseSettings::setClientMaxBodySize(const std::string& clientMaxBodySize)
{
	if (clientMaxBodySize.empty())
		throw (std::runtime_error("invalid number of arguments in \"client_max_body_size\" directive"));

	size_t	lastCharPos = clientMaxBodySize.find_first_not_of("0123456789");
	int 	factor = 1;
	std::string	numericStr;
	if (lastCharPos != std::string::npos && lastCharPos != clientMaxBodySize.size() - 1) 
	{
		if (lastCharPos != clientMaxBodySize.size() - 1)
			throw (std::runtime_error("\"client_max_body_size\" directive invalid value")); // Note 1
	}
	else if (lastCharPos != std::string::npos)
	{
		char	lastChar = std::tolower(clientMaxBodySize[lastCharPos]);
		if (lastChar != 'm' && lastChar != 'k' && lastChar != 'g')
			throw (std::runtime_error("\"client_max_body_size\" directive invalid value")); // Note 2

		if (lastCharPos != std::string::npos) {
			numericStr = clientMaxBodySize.substr(0, clientMaxBodySize.size() - 1);

			if (std::tolower(clientMaxBodySize[lastCharPos]) == 'k')
				factor = 1024;
			else if (std::tolower(clientMaxBodySize[lastCharPos]) == 'm')
				factor = 1024 * 1024; // 1,048,576
			else if (std::tolower(clientMaxBodySize[lastCharPos]) == 'g')
				factor = 1024 * 1024 * 1024; // 1,073,741,824
		}
	}
	else
		numericStr = clientMaxBodySize;

	std::stringstream ss(numericStr);
	size_t size = 0;

	ss >> size;
	if (ss.fail() || !ss.eof())
		throw (std::runtime_error("\"client_max_body_size\" directive invalid value")); // Note 3

	size *= factor;
	this->clientMaxBodySize = size;
}

/*
	GENERAL:
		Actual Nginx accepts these values as units:
			ms (milliseconds)
			s (seconds)
			m (minutes)
			h (hours)
			d (days)
		
		But for our purposes, we are not going to actually be keeping connections 
		open for days or hours. We will only implement seconds and actually specify 
		a max and min value for that timeout.
*/
void	BaseSettings::setKeepAliveTimeout(const std::string& keepAliveTimeout)
{
	std::string keepAliveValueStr;
	size_t pos = keepAliveTimeout.find_first_not_of("0123456789");

	if (pos != std::string::npos)
	{
		if (keepAliveTimeout.substr(pos) != "s")
			throw (std::runtime_error("\"keepalive_timeout\" directive invalid value"));
		keepAliveValueStr = keepAliveTimeout.substr(0, pos);
	}
	else
		keepAliveValueStr = keepAliveTimeout;

	std::stringstream 	ss(keepAliveValueStr);
	size_t				keepAliveTimeoutValue;

	ss >> keepAliveTimeoutValue;

	if (ss.fail() || !ss.eof())
		throw (std::runtime_error("\"keepalive_timeout\" directive invalid value"));
	if (keepAliveTimeoutValue < MIN_KEEPALIVE_TIMEOUT || keepAliveTimeoutValue > MAX_KEEPALIVE_TIMEOUT) {
        std::ostringstream oss;
        oss << MIN_KEEPALIVE_TIMEOUT << "s and " << MAX_KEEPALIVE_TIMEOUT << "s";
		throw (std::runtime_error("\"keepalive_timeout\" value must be between " + oss.str()));
	}
	this->keepaliveTimeout = keepAliveTimeoutValue;
}

/*
	RULES:
		Only one page allowed per error_page directive (everything else must be a status code)

		Error codes must be valid (between 300 and 599)

		Error code overrides are ignored within the same context but allowed accross different contexts

	NOTES:
		Note 1: if it's a brand new statusCode, then assign it to the errorPages map

		Note 2: this measn that the error code has been assigned before and is already
				present somewhere in the errorPages map.

		Note 3: we also have another map that runs side-by-side with errorPage map and
				that map is the errorPagesLevel map. So let's say we are talking about 
				status code 404. errorPage map already has a 404 code assigned to a 
				particular page. The errorPagesLevel map then goes and checks in which 
				context has that errorPagesLevel been assigned to. If the context we are 
				currently in is different to the one where the status code has previously 
				been assigned, then we accept it, and override it. Else, we ignore it. Also,
				as an extra piece of info, when we do errorPagesLevel[statusCode] = errorPagesContext;
				we are changing the value from the previous context to the current context on the 
				errorPagesLevel map, so this level's errorPagesLevel map is updated.

*/
void	BaseSettings::setErrorPages(const std::vector<std::string>& errorArgs, const std::string& errorPagesContext)
{
	std::string pageURI = errorArgs[errorArgs.size() - 1];
	for (size_t i = 0; i < errorArgs.size() - 1; i++) {
		int statusCode;
		std::stringstream ss(errorArgs[i]);

		ss >> statusCode;
		if (ss.fail() || !ss.eof())
			throw (std::runtime_error("invalid value \"" + errorArgs[i] + "\" in \"error_pages\" directive"));

		if (statusCode < 300 || statusCode > 599) {
			std::ostringstream oss;
			oss << statusCode;
			throw (std::runtime_error("value \"" + oss.str() + "\" must be between 300 and 599"));
		}

		if (errorPages.find(statusCode) == errorPages.end()) { // Note 1
			errorPages[statusCode] = pageURI;
			errorPagesLevel[statusCode] = errorPagesContext;
		}
		else												// Note 2
		{
			if (errorPagesLevel[statusCode] != errorPagesContext) // Note 3
			{
				errorPages[statusCode] = pageURI;
				errorPagesLevel[statusCode] = errorPagesContext;
			}
		}
	}
}

/*
	RULES:
		Relative paths can be used anywhere, for example:
			index content/index.html random.html;
		
		But absolute paths (those that begin with a /) can only be used as the 
		last entry in the index directive, for example:
			index random.html /Users/randomGuy/Desktop/website/content/www/index.html;
		
		Meaning that the following is NOT allowed:
			index /Users/randomGuy/Desktop/website/content/www/index.html random.html;
*/
void	BaseSettings::setIndex(const std::vector<std::string>& indexArgs)
{
	for (size_t i = 0; i < indexArgs.size() - 1; i++) {
		if (indexArgs[i][0] == '/')
			throw (std::runtime_error("only the last index in \"index\" directive should be absolute"));
	}
	this->index = indexArgs;
}

void	BaseSettings::setReturn(const std::vector<std::string>& returnArgs)
{
	std::stringstream ss(returnArgs[0]);
	int	statusCode;

	ss >> statusCode;
	if (returnArgs.size() > 1) 
	{
		if (ss.fail() || !ss.eof())
			throw (std::runtime_error("invalid return code \"" + returnArgs[0] + "\""));
		if (statusCode < 0 || statusCode > 999)
			throw (std::runtime_error("invalid return code \"" + returnArgs[0] + "\""));
		returnDirective.setStatusCode(statusCode);
		returnDirective.setTextOrURL(returnArgs[1]);
	}
	else
	{
		if (ss.fail() || !ss.eof()) {
			if ((returnArgs[0].size() >= 7) && (returnArgs[0].substr(0, 7) == "http://" || returnArgs[0].substr(0, 8) == "https://"))
				return (returnDirective.setTextOrURL(returnArgs[0]), void());
			else
				throw (std::runtime_error("invalid return code \"" + returnArgs[0] + "\""));
		}
		if (statusCode < 0 || statusCode > 999)
			throw (std::runtime_error("invalid return code \"" + returnArgs[0] + "\""));
		returnDirective.setStatusCode(statusCode);
	}
}

const std::string& BaseSettings::getRoot() const
{
	return (root);
}

const std::string& BaseSettings::getAutoindex() const
{
	return (autoIndex);
}

const std::map<int, std::string>& BaseSettings::getErrorPages() const
{
	return (errorPages);
}

const std::map<int, std::string>& BaseSettings::getErrorPagesLevel() const
{
	return (errorPagesLevel);
}

const std::vector<std::string>& BaseSettings::getIndex() const
{
	return (index);
}

const size_t& BaseSettings::getClientMaxBodySize() const
{
	return (clientMaxBodySize);
}

const size_t& BaseSettings::getKeepaliveTimeout() const
{
	return (keepaliveTimeout);
}

const ReturnDirective& BaseSettings::getReturnDirective() const
{
	return (returnDirective);
}

void BaseSettings::debugger() const
{
	// Print root and autoIndex
	std::cout << "root: " << root << std::endl;
	std::cout << "autoIndex: " << autoIndex << std::endl;

	// Print clientMaxBodySize
	std::cout << "clientMaxBodySize: " << clientMaxBodySize << std::endl;

	// Print errorPages
	std::cout << "errorPages:" << std::endl;
	for (std::map<int, std::string>::const_iterator it = errorPages.begin(); it != errorPages.end(); ++it) {
		std::cout << "  [" << it->first << "] = " << it->second << std::endl;
	}

	// Print errorPagesLevel
	std::cout << "errorPagesLevel:" << std::endl;
	for (std::map<int, std::string>::const_iterator it = errorPagesLevel.begin(); it != errorPagesLevel.end(); ++it) {
		std::cout << "  [" << it->first << "] = " << it->second << std::endl;
	}

	// Print index
	std::cout << "index:" << std::endl;
	for (std::vector<std::string>::const_iterator it = index.begin(); it != index.end(); ++it) {
		std::cout << "  " << *it << std::endl;
	}

	std::cout << "returnDirective:" << std::endl;
	std::cout << "    statusCode: " << returnDirective.getStatusCode() << std::endl;
	std::cout << "    textOrURL : " << returnDirective.getTextOrURL() << std::endl;

}
