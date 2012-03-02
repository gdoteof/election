$(document).ready(function() {
    var timeouts = {};
    var MESSAGES = {
        "incomplete": '<p class="incomplete">These are incomplete results.</p>',
        "complete": '<p class="complete">These are complete but unofficial results.</p>'
    };
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
            var uri = $(element).data('link');
            if (!(uri in timeouts)) {
                timeouts[uri] = {};
            }
            //TODO: Default refresh time?
            updateBallotItemArticle(uri, $(element), 60000, cache);
        });
    }

    function updateBallotItemArticle(uri, element, refreshTime, cache) {
        cache = typeof(cache) != 'undefined' ? cache : true;
        if (element.is(':hidden')) {
            clearTimeoutsByUri(uri);
            var timeout = setTimeout(function() {
                updateBallotItemArticle(uri, element, refreshTime);                
            }, refreshTime + 1000);
            timeouts[uri][timeout] = null;
            return;
        }
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
                    //TODO: "complete" should be at overall results level
                    var complete = null;
                    for (result in data.results) {
                        var resultElement = templateRow.clone();
                        resultElement.find(".option").text(result);
                        //TODO: Format votes
                        resultElement.find(".votes").text(data.results[result].votes);
                        resultElement.find(".percent").text(data.results[result].percent);
                        if (data.results[result].winner) {
                            resultElement.addClass("winner");
                        }
                        if (data.results[result].complete) {
                            resultElement.addClass("complete");
                            if (null === complete) {
                                complete = true;
                            }
                        } else if (false === data.results[result].complete) {
                            complete = false;
                        }
                        tableBody.append(resultElement);
                    }
                    element.find("footer .incomplete").remove();
                    element.find("footer .complete").remove();
                    //TODO: This check should not be necessary
                    if (0 == element.find("footer .partial").length) {
                        if (complete) {
                            element.find("footer").prepend(MESSAGES.complete);
                        } else if (false === complete) {
                            element.find("footer").prepend(MESSAGES.incomplete);
                        }
                    }
                }
                var date = new Date(jqXHR.getResponseHeader("Date"));
                //TODO: Handle timezone difference between server and client
                element.find(".updated").text($.format.date(date, "MMM d, yyyy K:mm:ss a"));
                var expires = new Date(jqXHR.getResponseHeader("Expires"));
                var expiration = expires.valueOf() - date.valueOf();
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
