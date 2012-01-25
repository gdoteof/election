$(document).ready(function() {
    var timeouts = {};
    updateBallotItemArticles();

    $("a.refresh").click(function(e) {
        e.preventDefault();
        updateBallotItemArticles(false);
    });

    function clearTimeoutsByUri(uri) {
        for (timeout in timeouts[uri]) {
            clearTimeout(timeout);
            delete timeouts[uri][timeout];
        }
    }

    function updateBallotItemArticles(cache) {
        $("article.ballot-item-results").each(function(index, element) {
            var uri = $(element).find('h1 a').first().attr("href");
            if (!(uri in timeouts)) {
                timeouts[uri] = {};
            }
            //TODO: Default refresh time?
            updateBallotItemArticle(uri, $(element), 60000, cache);
            //TODO: Indicate when results were udpated for each item, e.g. "Results updated within the last five minutes."
        });
    }

    function updateBallotItemArticle(uri, element, refreshTime, cache) {
        cache = typeof(cache) != 'undefined' ? cache : true;
        $.ajax({
            url: uri,
            dataType: 'json',
            ifModified: true,
            timeout: 30000,
            cache: cache,
            success: function(data, textStatus, jqXHR) {
                if (304 != jqXHR.status) {
                    var templateRow = element.find("thead tr.result").clone();
                    templateRow.find("th").replaceWith(function() {
                        return "<td class=\"" + $(this).attr("class") + "\">" + $(this).html() + "</td>";
                    });
                    var tableBody = element.find("tbody").empty();
                    for (result in data.results) {
                        var resultElement = templateRow.clone();
                        resultElement.find(".option").text(result);
                        //TODO: Format votes
                        resultElement.find(".votes").text(data.results[result].votes);
                        resultElement.find(".percent").text(data.results[result].percent);
                        tableBody.append(resultElement);
                    }
                }
                var date = jqXHR.getResponseHeader("Date");
                //TODO: Formate date
                element.find(".updated").text(date);
                var expires = jqXHR.getResponseHeader("Expires");
                var expiration = Date.parse(expires) - Date.parse(date);
                if (expiration <= 0) {
                    expiration = refreshTime;
                }
                clearTimeoutsByUri(uri);
                var timeout = setTimeout(function() {
                    updateBallotItemArticle(uri, element, expiration);                
                }, expiration + 1000);
                timeouts[uri][timeout] = null;
            },
            error: function(jqXHR, textStatus, errorThrown) {
                switch (jqXHR.status) {
                    case 503:
                        var retryAfter = jqXHR.getResponseHeader("Retry-After");
                        var date = jqXHR.getResponseHeader("Date");
                        var retry = parseInt(retryAfter) * 1000;
                        if (isNaN(retry)) {
                            retry = Date.parse(retryAfter) - Date.parse(date);
                        }
                        if (isNaN(retry)) {
                            retry = refreshTime;
                        }
                        if (retry <= 0) {
                            retry = refreshTime;
                        }
                        clearTimeoutsByUri(uri);
                        var timeout = setTimeout(function() {
                            updateBallotItemArticle(uri, element, retry);                
                        }, retry + 1000);
                        timeouts[uri][timeout] = null;
                        break;
                    case 400:
                    case 403:
                    case 404:
                    case 410:
                        // Do not repeat the request
                        break;
                    default:
                        clearTimeoutsByUri(uri);
                        var timeout = setTimeout(function() {
                            updateBallotItemArticle(uri, element, refreshTime);                
                        }, refreshTime + 1000);
                        timeouts[uri][timeout] = null;
                        break;
                }
            }
        });
    }
});
